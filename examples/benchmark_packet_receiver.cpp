#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv)
{
    using c64 = std::complex<float>;

    if ((argc < 1) || (argc > 3)) {
        fmt::println(
            stderr, "usage: {} [syncword_freq_bins] [syncword_threshold]", argv[0]);
        fmt::println(stderr, "");
        fmt::println(stderr, "the default syncword freq bins is 4");
        fmt::println(stderr, "the default syncword threshold is 9.5");
        std::exit(1);
    }
    const int syncword_freq_bins = argc >= 2 ? std::stoi(argv[1]) : 4;
    const float syncword_threshold = argc >= 3 ? std::stof(argv[2]) : 9.5f;

    const size_t samples_per_symbol = 4U;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::NullSource<c64>>();
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    const bool header_debug = false;
    const bool zmq_output = false;
    const bool log = false;
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

    if (fg.connect<"out">(source).to<"in">(*packet_receiver.syncword_detection) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(source).to<"in">(probe_rate) != gr::ConnectionResult::SUCCESS) {
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
