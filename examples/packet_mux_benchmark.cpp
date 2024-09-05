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

    if (argc != 7) {
        fmt::println(stderr,
                     "usage: {} mux_num_inputs packet_length "
                     "tagged_to_pdu_output_buffer_size mux_in_max_samples"
                     "mux_output_buffer_size pdu_to_stream_in_max_samples",
                     argv[0]);
        fmt::println(stderr, "example usage: {} 2 6000 0 0 0 0", argv[0]);
        std::exit(1);
    }

    // Test parameters
    using T = std::complex<float>;
    const size_t mux_num_inputs = std::stoul(argv[1]);
    const uint64_t packet_length = std::stoul(argv[2]);
    // 0UZ means do not resizeBuffer()
    const size_t tagged_to_pdu_output_buffer_size = std::stoul(argv[3]);
    // 0UZ means do not set max_samples
    const size_t mux_in_max_samples = std::stoul(argv[4]);
    // 0UZ means do not resizeBuffer()
    const size_t mux_output_buffer_size = std::stoul(argv[5]);
    // 0UZ means do not set max_samples
    const size_t pdu_to_stream_in_max_samples = std::stoul(argv[6]);

    gr::Graph fg;

    auto& mux = fg.emplaceBlock<PacketMux<Pdu<T>>>({ { "num_inputs", mux_num_inputs } });
    if (mux_in_max_samples != 0UZ) {
        for (auto& port : mux.in) {
            port.max_samples = mux_in_max_samples;
        }
    }
    if (mux_output_buffer_size != 0UZ) {
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        mux.out.resizeBuffer(mux_output_buffer_size))));
    }

    for (const auto j : std::views::iota(0UZ, mux_num_inputs)) {
        auto& source = fg.emplaceBlock<NullSource<T>>();
        auto& to_tagged = fg.emplaceBlock<StreamToTaggedStream<T>>(
            { { "packet_length", packet_length } });
        auto& to_pdu = fg.emplaceBlock<TaggedStreamToPdu<T>>();
        if (tagged_to_pdu_output_buffer_size != 0UZ) {
            expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                            to_pdu.out.resizeBuffer(tagged_to_pdu_output_buffer_size))));
        }
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect<"out">(source).to<"in">(to_tagged))));
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect<"out">(to_tagged).to<"in">(to_pdu))));
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect(to_pdu, "out"s, mux, fmt::format("in#{}", j)))));
    }

    auto& pdu_to_stream = fg.emplaceBlock<PduToTaggedStream<T>>();
    if (pdu_to_stream_in_max_samples != 0UZ) {
        pdu_to_stream.in.max_samples = pdu_to_stream.in.max_samples;
    }
    auto& sink = fg.emplaceBlock<NullSink<T>>();
    auto& probe_rate = fg.emplaceBlock<ProbeRate<T>>();
    auto& message_debug = fg.emplaceBlock<MessageDebug>();

    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"out">(mux).to<"in">(pdu_to_stream))));
    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"out">(pdu_to_stream).to<"in">(sink))));
    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"out">(pdu_to_stream).to<"in">(probe_rate))));
    expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                    fg.connect<"rate">(probe_rate).to<"print">(message_debug))));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    expect(ret.has_value());
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
