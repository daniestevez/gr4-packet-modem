#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/packet_mux.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <complex>

int main()
{
    using c64 = std::complex<float>;
    using namespace std::string_literals;
    using namespace gr::packet_modem;
    using namespace gr;

    const std::string packet_len_tag_key = "packet_len";
    constexpr auto connection_error = "connection error";

    gr::Graph fg;

    // test parameters
    const uint64_t syncword_length = 64;
    const uint64_t packet_length = 1500 * 4;

    auto& syncword_source = fg.emplaceBlock<NullSource<c64>>();
    auto& syncword_to_tagged = fg.emplaceBlock<StreamToTaggedStream<c64>>(
        { { "packet_length", syncword_length } });

    auto& packet_source = fg.emplaceBlock<NullSource<c64>>();
    auto& packet_to_tagged = fg.emplaceBlock<StreamToTaggedStream<c64>>(
        { { "packet_length", packet_length } });

    auto& symbols_mux = fg.emplaceBlock<PacketMux<c64>>({ { "num_inputs", 2UZ } });

    auto& probe_rate = fg.emplaceBlock<ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<MessageDebug>();

    if (fg.connect<"out">(syncword_source).to<"in">(syncword_to_tagged) !=
        ConnectionResult::SUCCESS) {
        throw std::runtime_error(connection_error);
    }
    if (fg.connect(syncword_to_tagged, "out"s, symbols_mux, "in#0"s) !=
        ConnectionResult::SUCCESS) {
        throw std::runtime_error(connection_error);
    }
    if (fg.connect<"out">(packet_source).to<"in">(packet_to_tagged) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect(packet_to_tagged, "out"s, symbols_mux, "in#1"s) !=
        ConnectionResult::SUCCESS) {
        throw std::runtime_error(connection_error);
    }
    if (fg.connect<"out">(symbols_mux).to<"in">(probe_rate) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
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
