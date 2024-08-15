#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_source.hpp>
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
    using namespace std::string_literals;

    if (argc != 2) {
        fmt::println(stderr, "usage: {} input_file", argv[0]);
        std::exit(1);
    }

    gr::Graph fg;
    auto& file_source =
        fg.emplaceBlock<gr::packet_modem::FileSource<c64>>({ { "filename", argv[1] } });
    const size_t samples_per_symbol = 4;
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

    const char* connection_error = "connection error";

    if (fg.connect(file_source, "out"s, *packet_receiver.syncword_detection, "in"s) !=
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
        fmt::println(stderr, "scheduler error: {}", ret.error());
        std::exit(1);
    }

    return 0;
}
