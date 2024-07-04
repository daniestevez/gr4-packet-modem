#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_resampler.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_taps.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <numbers>

boost::ut::suite PfbArbResamplerTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "pfb_arb_resampler_complex_exponential"_test = [] {
        Graph fg;
        using c64 = std::complex<float>;
        const size_t num_items = 100000;
        const double freq = 0.01;
        double phase = 0.0;
        std::vector<c64> v(num_items);
        for (size_t j = 0; j < num_items; ++j) {
            v[j] = c64{ static_cast<float>(std::cos(phase)),
                        static_cast<float>(std::sin(phase)) };
            phase += freq;
            if (phase >= std::numbers::pi) {
                phase -= 2 * std::numbers::pi;
            }
        }
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        const double resampling_rate = 1.1234;
        auto& resampler =
            fg.emplaceBlock<gr::packet_modem::PfbArbResampler<c64, c64, float, double>>(
                { { "taps", pfb_arb_taps }, { "rate", resampling_rate } });
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(resampler)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(resampler).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        const size_t expected_data_size =
            static_cast<size_t>(static_cast<double>(num_items) * resampling_rate);
        // use some margin for the number of output samples
        expect(data.size() <= expected_data_size + 5);
        expect(expected_data_size <= data.size() + 5);
        phase = 0.0;
        const double out_freq = freq / resampling_rate;
        const float tolerance = 3e-3f;
        // only compare elements after the filter transient
        const size_t start = 1000;
        for (size_t j = start; j < data.size(); ++j) {
            if (j == start) {
                // take phase delay measurement
                phase = static_cast<double>(std::arg(data[j]));
            } else {
                const c64 expected = c64{ static_cast<float>(std::cos(phase)),
                                          static_cast<float>(std::sin(phase)) };
                // check output
                expect(std::abs(data[j] - expected) < tolerance);
            }
            phase += out_freq;
            if (phase >= std::numbers::pi) {
                phase -= 2 * std::numbers::pi;
            }
        }
    };
};

int main() {}
