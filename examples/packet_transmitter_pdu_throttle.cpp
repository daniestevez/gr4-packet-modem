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
    const double samp_rate = 100e3;
    const uint64_t packet_length = 1500;
    std::vector<uint8_t> v(packet_length);
    std::iota(v.begin(), v.end(), 0);
    auto& strobe =
        fg.emplaceBlock<gr::packet_modem::ItemStrobe<gr::packet_modem::Pdu<uint8_t>>>(
            { { "interval_secs", 1.0 }, { "sleep", false } });
    strobe.item = { v, {} };
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
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000UZ } });
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              strobe.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(packet_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(throttle).to<"in">(probe_rate)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, probe_rate.rate.connect(message_debug.print)));

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
