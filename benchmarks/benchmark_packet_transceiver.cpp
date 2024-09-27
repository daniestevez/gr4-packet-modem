#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv)
{
    using c64 = std::complex<float>;

    if ((argc < 2) || (argc > 5)) {
        fmt::println(
            stderr,
            "usage: {} stream_mode [syncword_freq_bins] [syncword_threshold] [log]",
            argv[0]);
        fmt::println(stderr, "");
        fmt::println(stderr, "the default syncword freq bins is 4");
        fmt::println(stderr, "the default syncword threshold is 9.5");
        fmt::println(stderr, "the default is log = 0");
        std::exit(1);
    }
    const bool stream_mode = std::stoi(argv[1]) != 0;
    const int syncword_freq_bins = argc >= 3 ? std::stoi(argv[2]) : 4;
    const float syncword_threshold = argc >= 4 ? std::stof(argv[3]) : 9.5f;
    const bool log = argc >= 5 ? std::stoi(argv[4]) : 0;

    const size_t samples_per_symbol = 4U;
    const size_t packet_size = 1500UZ;

    gr::Graph fg;
    const std::vector<uint8_t> packet(packet_size);
    const gr::packet_modem::Pdu<uint8_t> pdu = { packet, {} };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<uint8_t>>>(
            { { "repeat", true } });
    source.data = std::vector<gr::packet_modem::Pdu<uint8_t>>{ pdu };
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    const bool header_debug = false;
    const bool zmq_output = false;
    auto packet_receiver = gr::packet_modem::PacketReceiver(fg,
                                                            samples_per_symbol,
                                                            "packet_len",
                                                            header_debug,
                                                            zmq_output,
                                                            log,
                                                            syncword_freq_bins,
                                                            syncword_threshold);
    auto& sink = fg.emplaceBlock<gr::packet_modem::NullSink<uint8_t>>();

    const char* connection_error = "connection_error";

    if (stream_mode) {
        if (fg.connect<"out">(*packet_transmitter_pdu.rrc_interp)
                .to<"in">(*packet_receiver.syncword_detection) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(*packet_transmitter_pdu.rrc_interp).to<"in">(probe_rate) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    } else {
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
        if (fg.connect<"out">(packet_to_stream)
                .to<"in">(*packet_receiver.syncword_detection) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(packet_to_stream).to<"in">(probe_rate) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        auto& count_sink = fg.emplaceBlock<gr::packet_modem::NullSink<gr::Message>>();
        if (fg.connect<"count">(packet_to_stream).to<"in">(count_sink) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    }

    if (fg.connect<"out">(source).to<"in">(*packet_transmitter_pdu.ingress) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"rate">(probe_rate).to<"print">(message_debug) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(*packet_receiver.payload_crc_check).to<"in">(sink) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded> sched{ std::move(
        fg) };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
        std::exit(1);
    }

    return 0;
}
