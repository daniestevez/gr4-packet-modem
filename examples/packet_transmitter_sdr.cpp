#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/tun_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char* argv[])
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    // The first command line argument of this example indicates the file to
    // write the output to.
    expect(fatal(argc == 2));

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun_tx" }, { "netns_name", "gr4_tx" } });
    const bool stream_mode = false;
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& packet_to_stream =
        fg.emplaceBlock<gr::packet_modem::PacketToStream<gr::packet_modem::Pdu<c64>>>();
    // Limit the number of samples that packet_to_stream can produce in one
    // call. Otherwise it produces 65536 items on the first call, and then "it
    // gets behind the Throttle block" by these many samples.
    packet_to_stream.out.max_samples = 1000U;
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    expect(eq(gr::ConnectionResult::SUCCESS,
              source.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(packet_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
