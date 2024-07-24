#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/costas_loop.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <numbers>
#include <random>

boost::ut::suite CostasLoopTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "costas_loop_convergence"_test = [](auto constellation) {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        using c64 = std::complex<float>;
        std::vector<c64> v;
        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<uint8_t> dist(0, 3);
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            c64 z{ 1.0f, 0.0f };
            if (constellation == "BPSK") {
                if (dist(e) % 2 != 0) {
                    z = -z;
                }
            } else if (constellation == "QPSK") {
                const uint8_t n = dist(e);
                const float a = 1.0f / std::numbers::sqrt2_v<float>;
                z = c64{ n % 2 == 0 ? a : -a, n / 2 == 0 ? a : -a };
            }
            v.push_back(z);
        }
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        auto& rotator = fg.emplaceBlock<Rotator<>>({ { "phase_incr", 0.01f } });
        auto& costas =
            fg.emplaceBlock<CostasLoop<>>({ { "constellation", constellation } });
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(rotator)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(rotator).to<"in">(costas)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect(costas, "out"s, sink, "in"s)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        const size_t start = 1000UZ;
        const float tolerance = 1e-2f;
        // This assumes that the Costas loop doesn't slip
        // half-cycles/quarter-cycles while locking
        for (size_t j = start; j < static_cast<size_t>(num_items); ++j) {
            const c64 w = data[j] * std::conj(v[j]);
            expect(std::abs(w - c64{ 1.0f, 0.0f }) < tolerance);
        }
        expect(sink.tags().empty());
    } | std::vector<std::string>{ "PILOT", "BPSK", "QPSK" };
};

int main() {}
