#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_counter.hpp>
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
    // write the output to. The second indicates whether to use stream_mode or
    // not.
    expect(fatal(argc == 3));
    const std::string filename = argv[1];
    const bool stream_mode = std::stod(argv[2]) != 0;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun_tx" },
          { "netns_name", "gr4_tx" },
          { "max_packets", 2UZ },
          { "idle_packet_size", stream_mode ? 256UZ : 0UZ } });
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", filename } });

    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(source, "out"s, *packet_transmitter_pdu.ingress, "in"s)));

    if (stream_mode) {
        auto& packet_counter = fg.emplaceBlock<gr::packet_modem::PacketCounter<c64>>(
            { { "drop_tags", true } });
        packet_counter.out._ioHandler._debug = true;
        sink.in._ioHandler._debug = true;
        expect(
            eq(gr::ConnectionResult::SUCCESS,
               fg.connect(
                   *packet_transmitter_pdu.rrc_interp, "out"s, packet_counter, "in"s)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"count">(packet_counter).to<"count">(source)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_counter).to<"in">(sink)));
    } else {
        auto& packet_to_stream = fg.emplaceBlock<
            gr::packet_modem::PacketToStream<gr::packet_modem::Pdu<c64>>>();
        // Limit the number of samples that packet_to_stream can produce in one
        // call. Otherwise it produces 65536 items on the first call, and then "it
        // gets behind the Throttle block" by these many samples.
        packet_to_stream.out.max_samples = 1000U;
        expect(eq(
            gr::ConnectionResult::SUCCESS,
            fg.connect(
                *packet_transmitter_pdu.burst_shaper, "out"s, packet_to_stream, "in"s)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_to_stream).to<"in">(sink)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"count">(packet_to_stream).to<"count">(source)));
    }

    gr::scheduler::Simple sched{ std::move(fg) };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
