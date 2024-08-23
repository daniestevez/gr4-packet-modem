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
#include <complex>
#include <cstdint>

int main(int argc, char* argv[])
{
    using c64 = std::complex<float>;

    if (argc != 4) {
        fmt::println(stderr, "usage: {} output_file samp_rate stream_mode", argv[0]);
        std::exit(1);
    }
    const double samp_rate = std::stod(argv[2]);
    const bool stream_mode = std::stod(argv[3]) != 0;

    gr::Graph fg;
    const uint64_t packet_length = 1500;
    std::vector<uint8_t> v(packet_length);
    std::iota(v.begin(), v.end(), 0);
    // auto& source =
    //     fg.emplaceBlock<gr::packet_modem::ItemStrobe<gr::packet_modem::Pdu<uint8_t>>>(
    //         { { "interval_secs", 0.05 }, { "sleep", false } });
    // source.item = { v, {} };
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun" },
          { "netns_name", "gr4_tx" },
          { "idle_packet_size", stream_mode ? 256UZ : 0UZ } });
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000UZ } });
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();

    const char* connection_error = "connection error";

    if (fg.connect<"out">(source).to<"in">(*packet_transmitter_pdu.ingress) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (!stream_mode) {
        auto& packet_to_stream = fg.emplaceBlock<
            gr::packet_modem::PacketToStream<gr::packet_modem::Pdu<c64>>>();
        // Limit the number of samples that packet_to_stream can produce in one
        // call. Otherwise it produces 65536 items on the first call, and then "it
        // gets behind the Throttle block" by these many samples.
        packet_to_stream.out.max_samples = 1000U;

        if (fg.connect<"out">(*packet_transmitter_pdu.burst_shaper)
                .to<"in">(packet_to_stream) != gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(packet_to_stream).to<"in">(throttle) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    } else {
        if (fg.connect<"out">(*packet_transmitter_pdu.rrc_interp).to<"in">(throttle) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    }
    if (fg.connect<"out">(throttle).to<"in">(sink) != gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(throttle).to<"in">(probe_rate) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"rate">(probe_rate).to<"print">(message_debug) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }

    // This doesn't work if the scheduler is switched to multiThreaded. The
    // Throttle block only gets non-zero items for the first packet, even though
    // the PacketToStream block is producing non-zero items for each packet.
    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println(stderr, "scheduler error: {}", ret.error());
        std::exit(1);
    }

    return 0;
}
