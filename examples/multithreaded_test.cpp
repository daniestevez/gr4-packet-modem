#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/packet_mux.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>
#include <numbers>
#include <numeric>

int main(int argc, char* argv[])
{
    using namespace boost::ut;
    using namespace std::string_literals;
    using namespace gr::packet_modem;

    if (argc != 2) {
        fmt::println(stderr, "usage: {} use_multithreaded", argv[0]);
        std::exit(1);
    }

    const bool use_multithreaded = std::stoi(argv[1]);

    // Test parameters
    using T = std::complex<float>;
    const size_t mux_num_inputs = 2;
    const uint64_t packet_length = 2500;

    gr::Graph fg;

    auto& mux = fg.emplaceBlock<PacketMux<Pdu<T>>>({ { "num_inputs", mux_num_inputs } });

    for (const auto j : std::views::iota(0UZ, mux_num_inputs)) {
        auto& source = fg.emplaceBlock<NullSource<T>>();
        auto& to_tagged = fg.emplaceBlock<StreamToTaggedStream<T>>(
            { { "packet_length", packet_length } });
        auto& to_pdu = fg.emplaceBlock<TaggedStreamToPdu<T>>();
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect<"out">(source).to<"in">(to_tagged))));
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect<"out">(to_tagged).to<"in">(to_pdu))));
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect(to_pdu, "out"s, mux, fmt::format("in#{}", j)))));
    }

    auto& pdu_to_stream = fg.emplaceBlock<PduToTaggedStream<T>>();
    auto& sink = fg.emplaceBlock<NullSink<T>>();

    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"out">(mux).to<"in">(pdu_to_stream))));
    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"out">(pdu_to_stream).to<"in">(sink))));

    if (use_multithreaded) {
        fmt::println(
            "using gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded>");
        gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded> sched{
            std::move(fg)
        };
        const auto ret = sched.runAndWait();
        expect(ret.has_value());
        if (!ret.has_value()) {
            fmt::println("scheduler error: {}", ret.error());
        }
    } else {
        fmt::println(
            "using "
            "gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded>");
        gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
            std::move(fg)
        };
        const auto ret = sched.runAndWait();
        expect(ret.has_value());
        if (!ret.has_value()) {
            fmt::println("scheduler error: {}", ret.error());
        }
    }

    return 0;
}
