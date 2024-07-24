#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>
#include <complex>

boost::ut::suite NoiseSourceTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "noise_source_gaussian_power"_test = []<typename T> {
        Graph fg;
        constexpr auto num_items = 1000000_ul;
        const float amplitude = 1.234f;
        auto& source = fg.emplaceBlock<NoiseSource<T>>(
            { { "noise_type", "gaussian" }, { "amplitude", amplitude } });
        auto& head =
            fg.emplaceBlock<Head<T>>({ { "num_items", static_cast<size_t>(num_items) } });
        auto& sink = fg.emplaceBlock<VectorSink<T>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect(source, "out"s, head, "in"s)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect(head, "out"s, sink, "in"s)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        float power = 0;
        for (auto x : data) {
            float x_power;
            if constexpr (std::is_floating_point_v<T>) {
                x_power = x * x;
            } else {
                // complex
                x_power = x.real() * x.real() + x.imag() * x.imag();
            }
            power += x_power;
        }
        power /= static_cast<float>(static_cast<size_t>(num_items));
        const float rel_tolerance = 1e-2f;
        expect(std::abs(power - amplitude * amplitude) <
               amplitude * amplitude * rel_tolerance);
        expect(sink.tags().empty());
    } | std::tuple<float, std::complex<float>>{};
};

int main() {}
