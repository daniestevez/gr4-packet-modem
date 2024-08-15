#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/packet_counter.hpp>
#include <gnuradio-4.0/packet-modem/packet_limiter.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/packet_type_filter.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_resampler.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_taps.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/tag_gate.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/tun_sink.hpp>
#include <gnuradio-4.0/packet-modem/tun_source.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv)
{
    using c64 = std::complex<float>;
    using namespace std::string_literals;

    if (argc != 5) {
        fmt::println(
            stderr, "usage: {} esn0_db cfo_rad_samp sfo_ppm stream_mode", argv[0]);
        std::exit(1);
    }
    const double esn0_db = std::stod(argv[1]);
    const float freq_error = std::stof(argv[2]);
    const float sfo_ppm = std::stof(argv[3]);
    const bool stream_mode = std::stod(argv[4]) != 0;

    const double tx_power = 0.32; // measured from packet_transmitter_pdu output
    const size_t samples_per_symbol = 4U;
    const double n0 = tx_power * static_cast<double>(samples_per_symbol) *
                      std::pow(10.0, -0.1 * esn0_db);
    const float noise_amplitude = static_cast<float>(std::sqrt(n0));

    const double samp_rate = 3.2e6;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun_tx" },
          { "netns_name", "gr4_tx" },
          { "max_packets", 2UZ },
          { "idle_packet_size", stream_mode ? 256UZ : 0UZ } });
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000UZ } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    auto& resampler = fg.emplaceBlock<gr::packet_modem::PfbArbResampler<c64, c64, float>>(
        { { "taps", gr::packet_modem::pfb_arb_taps },
          { "rate", 1.0f + 1e-6f * sfo_ppm } });
    auto& rotator =
        fg.emplaceBlock<gr::packet_modem::Rotator<>>({ { "phase_incr", freq_error } });
    auto& noise_source = fg.emplaceBlock<gr::packet_modem::NoiseSource<c64>>(
        { { "noise_type", "gaussian" }, { "amplitude", noise_amplitude } });
    auto& add_noise = fg.emplaceBlock<gr::packet_modem::Add<c64>>();
    const bool header_debug = false;
    const bool zmq_output = true;
    const bool log = true;
    auto packet_receiver = gr::packet_modem::PacketReceiver(
        fg, samples_per_symbol, "packet_len", header_debug, zmq_output, log);
    auto& packet_type_filter = fg.emplaceBlock<gr::packet_modem::PacketTypeFilter<>>(
        { { "packet_type", "user_data" } });
    auto& tag_to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<uint8_t>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::TunSink>(
        { { "tun_name", "gr4_tun_rx" }, { "netns_name", "gr4_rx" } });

    const char* connection_error = "connection_error";

    if (stream_mode) {
        auto& packet_counter = fg.emplaceBlock<gr::packet_modem::PacketCounter<c64>>(
            { { "drop_tags", true } });
        if (fg.connect(
                *packet_transmitter_pdu.rrc_interp, "out"s, packet_counter, "in"s) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"count">(packet_counter).to<"count">(source) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(packet_counter).to<"in">(throttle) !=
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
        if (fg.connect(
                *packet_transmitter_pdu.burst_shaper, "out"s, packet_to_stream, "in"s) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(packet_to_stream).to<"in">(throttle) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"count">(packet_to_stream).to<"count">(source) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    }

    if (fg.connect(source, "out"s, *packet_transmitter_pdu.ingress, "in"s) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(throttle).to<"in">(resampler) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(throttle).to<"in">(probe_rate) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect(probe_rate, "rate"s, message_debug, "print"s) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(resampler).to<"in">(rotator) != gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(rotator).to<"in0">(add_noise) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(noise_source).to<"in1">(add_noise) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect(add_noise, "out"s, *packet_receiver.syncword_detection, "in"s) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect(
            *packet_receiver.payload_crc_check, "out"s, packet_type_filter, "in"s) !=
        gr::ConnectionResult::SUCCESS) {
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
        fmt::println("scheduler error: {}", ret.error());
        std::exit(1);
    }

    return 0;
}
