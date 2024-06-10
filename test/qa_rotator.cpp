#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <array>
#include <complex>
#include <numbers>

boost::ut::suite RotatorTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "rotator"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        const float phase_incr = 0.1f;
        using c64 = std::complex<float>;
        std::vector<c64> v(static_cast<size_t>(num_items), c64{ 1.0f, 0.0f });
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        auto& rotator = fg.emplaceBlock<Rotator<>>({ { "phase_incr", phase_incr } });
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(rotator)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(rotator).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        double phase = 0.0;
        for (const auto x : data) {
            const c64 expected = { static_cast<float>(std::cos(phase)),
                                   static_cast<float>(std::sin(phase)) };
            const float tolerance = 5e-4f;
            expect(std::abs(x - expected) < tolerance);
            phase += static_cast<double>(phase_incr);
            if (phase >= std::numbers::pi) {
                phase -= 2.0 * std::numbers::pi;
            }
        }
        expect(sink.tags().empty());
    };
};

int main() {}
