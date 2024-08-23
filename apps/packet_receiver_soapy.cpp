#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_type_filter.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/tun_sink.hpp>
#include <gnuradio-4.0/soapy/Soapy.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv)
{
    using c64 = std::complex<float>;

    if ((argc < 2) || (argc > 5)) {
        fmt::println(stderr,
                     "usage: {} rf_freq_hz [samp_rate_sps] [syncword_freq_bins] "
                     "[syncword_threshold]",
                     argv[0]);
        fmt::println(stderr, "");
        fmt::println(stderr, "the default sample rate is 3.2 Msps");
        fmt::println(stderr, "the default syncword freq bins is 4");
        fmt::println(stderr, "the default syncword threshold is 9.5");
        std::exit(1);
    }
    const double rf_freq = std::stod(argv[1]);
    const float samp_rate = argc >= 3 ? std::stof(argv[2]) : 3.2e6;
    const int syncword_freq_bins = argc >= 4 ? std::stoi(argv[3]) : 4;
    const float syncword_threshold = argc >= 5 ? std::stof(argv[4]) : 9.5;

    gr::Graph fg;
    auto& soapy_source = fg.emplaceBlock<gr::blocks::soapy::SoapyBlock<c64, 1UZ>>(
        { { "device", "rtlsdr" },
          { "sample_rate", samp_rate },
          { "rx_center_frequency", std::vector<double>{ rf_freq } },
          { "rx_gains", std::vector<double>{ 30.0 } } });
    const size_t samples_per_symbol = 4;
    const bool header_debug = false;
    const bool zmq_output = true;
    const bool log = true;
    auto packet_receiver = gr::packet_modem::PacketReceiver(fg,
                                                            samples_per_symbol,
                                                            "packet_len",
                                                            header_debug,
                                                            zmq_output,
                                                            log,
                                                            syncword_freq_bins,
                                                            syncword_threshold);
    auto& packet_type_filter = fg.emplaceBlock<gr::packet_modem::PacketTypeFilter<>>(
        { { "packet_type", "user_data" } });
    auto& tag_to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<uint8_t>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::TunSink>(
        { { "tun_name", "gr4_tun_rx" }, { "netns_name", "gr4_rx" } });

    const char* connection_error = "connection error";

    if (fg.connect<"out">(soapy_source).to<"in">(*packet_receiver.syncword_detection) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(*packet_receiver.payload_crc_check)
            .to<"in">(packet_type_filter) != gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(packet_type_filter).to<"in">(tag_to_pdu) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(tag_to_pdu).to<"in">(sink) != gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }

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
