#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char** argv)
{
    using c64 = std::complex<float>;

    if (argc != 2) {
        fmt::println(stderr, "usage: {} stream_mode", argv[0]);
        std::exit(1);
    }
    const bool stream_mode = std::stoi(argv[1]) != 0;

    const size_t samples_per_symbol = 4U;
    const size_t packet_size = 1500UZ;

    gr::Graph fg;
    const std::vector<uint8_t> packet(packet_size);
    const std::vector<gr::Tag> tags = { { 0, { { "packet_len", packet_size } } } };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>({ { "repeat", true } });
    source.data = packet;
    source.tags = tags;
    auto packet_transmitter =
        gr::packet_modem::PacketTransmitter(fg, stream_mode, samples_per_symbol);
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();

    const char* connection_error = "connection_error";

    if (fg.connect<"out">(source).to<"in">(*packet_transmitter.ingress) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (!stream_mode) {
        auto& packet_to_stream = fg.emplaceBlock<gr::packet_modem::PacketToStream<c64>>();
        // Limit the number of samples that packet_to_stream can produce in one
        // call. Otherwise it produces 65536 items on the first call, and then "it
        // gets behind the Throttle block" by these many samples.
        packet_to_stream.out.max_samples = 1000U;
        if (fg.connect<"out">(*packet_transmitter.burst_shaper)
                .to<"in">(packet_to_stream) != gr::ConnectionResult::SUCCESS) {
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
    } else {
        if (fg.connect<"out">(*packet_transmitter.rrc_interp_mult_tag)
                .to<"in">(probe_rate) != gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
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
