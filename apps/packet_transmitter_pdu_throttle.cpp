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
    using namespace std::string_literals;

    // The first command line argument of this example indicates the file to
    // write the output to.
    expect(fatal(argc == 2));

    gr::Graph fg;
    const double samp_rate = 10000e3;
    const uint64_t packet_length = 1500;
    std::vector<uint8_t> v(packet_length);
    std::iota(v.begin(), v.end(), 0);
    // auto& source =
    //     fg.emplaceBlock<gr::packet_modem::ItemStrobe<gr::packet_modem::Pdu<uint8_t>>>(
    //         { { "interval_secs", 0.05 }, { "sleep", false } });
    // source.item = { v, {} };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::TunSource>({ { "tun_name", "gr4_tun" } });
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
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000UZ } });
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(source, "out"s, *packet_transmitter_pdu.ingress, "in"s)));
    expect(
        eq(gr::ConnectionResult::SUCCESS,
           fg.connect(
               *packet_transmitter_pdu.burst_shaper, "out"s, packet_to_stream, "in"s)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(throttle).to<"in">(probe_rate)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(probe_rate, "rate"s, message_debug, "print"s)));

    // This doesn't work if the scheduler is switched to multiThreaded. The
    // Throttle block only gets non-zero items for the first packet, even though
    // the PacketToStream block is producing non-zero items for each packet.
    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
