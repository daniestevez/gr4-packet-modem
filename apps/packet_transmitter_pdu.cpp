#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <complex>
#include <cstdint>
#include <cstdlib>

int main(int argc, char* argv[])
{
    using c64 = std::complex<float>;

    if (argc != 3) {
        fmt::println(stderr, "usage: {} output_file stream_mode", argv[0]);
        std::exit(1);
    }

    const bool stream_mode = std::stod(argv[2]) != 0;

    gr::Graph fg;
    const uint64_t packet_length = 1500;
    const std::vector<uint8_t> v(packet_length);
    const gr::packet_modem::Pdu<uint8_t> pdu = { v, {} };
    auto& vector_source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<uint8_t>>>(
            { { "repeat", true } });
    vector_source.data = std::vector<gr::packet_modem::Pdu<uint8_t>>{ pdu };
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();

    const char* connection_error = "connection error";

    if (fg.connect<"out">(vector_source).to<"in">(*packet_transmitter_pdu.ingress) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (!stream_mode) {
        // do not produce tags in PduToTaggedStream
        auto& pdu_to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<c64>>(
            { { "packet_len_tag_key", "" } });
        if (max_in_samples) {
            pdu_to_stream.in.max_samples = max_in_samples;
        }
        if (fg.connect<"out">(*packet_transmitter_pdu.burst_shaper)
                .to<"in">(pdu_to_stream) != gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(pdu_to_stream).to<"in">(sink) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(pdu_to_stream).to<"in">(probe_rate) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    } else {
        if (fg.connect<"out">(*packet_transmitter_pdu.rrc_interp).to<"in">(sink) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
        if (fg.connect<"out">(*packet_transmitter_pdu.rrc_interp).to<"in">(probe_rate) !=
            gr::ConnectionResult::SUCCESS) {
            throw gr::exception(connection_error);
        }
    }
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    if (fg.connect<"rate">(probe_rate).to<"print">(message_debug) !=
        gr::ConnectionResult::SUCCESS) {
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
