#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char* argv[])
{
    using c64 = std::complex<float>;

    if (argc != 3) {
        fmt::println(stderr, "usage: {} output_file stream_mode", argv[0]);
        std::exit(1);
    }

    const bool stream_mode = std::stoi(argv[2]) != 0;

    gr::Graph fg;
    auto& random_source = fg.emplaceBlock<gr::packet_modem::RandomSource<uint8_t>>(
        { { "minimum", uint8_t{ 0 } },
          { "maximum", uint8_t{ 255 } },
          { "num_items", 1000000UZ },
          { "repeat", true } });
    random_source.name = "RandomSource";
    const uint64_t packet_length = 1500;
    auto& stream_to_tagged =
        fg.emplaceBlock<gr::packet_modem::StreamToTaggedStream<uint8_t>>(
            { { "packet_length", packet_length } });
    auto packet_transmitter = gr::packet_modem::PacketTransmitter(fg, stream_mode);
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();

    const char* connection_error = "connection error";

    if (fg.connect<"out">(random_source).to<"in">(stream_to_tagged) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(stream_to_tagged).to<"in">(*packet_transmitter.ingress) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (!stream_mode) {
        if (fg.connect<"out">(*packet_transmitter.burst_shaper).to<"in">(sink) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(*packet_transmitter.burst_shaper).to<"in">(probe_rate) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    } else {
        if (fg.connect<"out">(*packet_transmitter.rrc_interp_mult_tag).to<"in">(sink) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(*packet_transmitter.rrc_interp_mult_tag)
                .to<"in">(probe_rate) != gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    }
    if (fg.connect<"rate">(probe_rate).to<"print">(message_debug) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
