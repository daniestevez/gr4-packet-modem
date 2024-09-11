#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
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

    const std::vector<uint8_t> syncword = {
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
        uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }
    };
    auto rrc_taps = gr::packet_modem::firdes::root_raised_cosine(
        1.0,
        static_cast<double>(samples_per_symbol),
        1.0,
        0.35,
        samples_per_symbol * 11U);
    // normalize RRC taps to unity RMS norm
    float rrc_taps_norm = 0.0f;
    for (auto x : rrc_taps) {
        rrc_taps_norm += x * x;
    }
    rrc_taps_norm = std::sqrt(rrc_taps_norm);
    for (auto& x : rrc_taps) {
        x /= rrc_taps_norm;
    }
    const std::vector<c64> bpsk_constellation = { { 1.0f, 0.0f }, { -1.0f, 0.0f } };
    auto& syncword_detection = fg.emplaceBlock<gr::packet_modem::SyncwordDetection>(
        { { "rrc_taps", rrc_taps },
          { "syncword", syncword },
          { "constellation", bpsk_constellation },
          { "min_freq_bin", -syncword_freq_bins },
          { "max_freq_bin", syncword_freq_bins },
          { "power_threshold", syncword_threshold } });

    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();

    const char* connection_error = "connection_error";

    if (fg.connect<"out">(source).to<"in">(syncword_detection) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(syncword_detection).to<"in">(probe_rate) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"rate">(probe_rate).to<"print">(message_debug) !=
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
