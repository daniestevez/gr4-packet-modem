#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/coarse_frequency_correction.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <numbers>

boost::ut::suite CoarseFrequencyCorrectionTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "coarse_frequency_correction"_test = [] {
        Graph fg;
        using c64 = std::complex<float>;
        constexpr auto num_items = 10000_ul;
        const std::vector<c64> v(static_cast<size_t>(num_items), c64{ 1.0f, 0.0f });
        const std::vector<Tag> tags = {
            { 100, { { "syncword_freq", 0.1f } } },
            { 1000, { { "syncword_freq", 0.1f } } },
            { 1500, { { "syncword_freq", 0.1f } } },
            { 5000, { { "syncword_freq", 0.0f } } },
            { 6500, { { "syncword_freq", 0.2f } } },
        };
        auto& source = fg.emplaceBlock<VectorSource<c64>>();
        source.data = v;
        source.tags = tags;
        auto& freq_correction = fg.emplaceBlock<CoarseFrequencyCorrection<>>();
        auto& sink = fg.emplaceBlock<VectorSink<c64>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(freq_correction)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(freq_correction).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        // the block starts with a frequency of zero
        for (size_t j = 0; j < 100UZ; ++j) {
            expect(eq(data[j], c64{ 1.0f, 0.0f }));
        }
        float phase = 0.0f;
        float tolerance = 1e-3f;
        float freq = 0.1f;
        for (size_t j = 100UZ; j < 1000UZ; ++j) {
            const c64 z{ std::cos(phase), std::sin(phase) };
            expect(std::abs(data[j] - z) < tolerance);
            phase -= freq;
            if (phase < -std::numbers::pi_v<float>) {
                phase += 2.0f * std::numbers::pi_v<float>;
            }
        }
        phase = 0.0f;
        for (size_t j = 1000UZ; j < 1500UZ; ++j) {
            const c64 z{ std::cos(phase), std::sin(phase) };
            expect(std::abs(data[j] - z) < tolerance);
            phase -= freq;
            if (phase < -std::numbers::pi_v<float>) {
                phase += 2.0f * std::numbers::pi_v<float>;
            }
        }
        phase = 0.0f;
        for (size_t j = 1500UZ; j < 5000UZ; ++j) {
            const c64 z{ std::cos(phase), std::sin(phase) };
            expect(std::abs(data[j] - z) < tolerance);
            phase -= freq;
            if (phase < -std::numbers::pi_v<float>) {
                phase += 2.0f * std::numbers::pi_v<float>;
            }
        }
        for (size_t j = 5000UZ; j < 6500UZ; ++j) {
            expect(eq(data[j], c64{ 1.0f, 0.0f }));
        }
        phase = 0.0f;
        freq = 0.2f;
        for (size_t j = 6500UZ; j < static_cast<size_t>(num_items); ++j) {
            const c64 z{ std::cos(phase), std::sin(phase) };
            expect(std::abs(data[j] - z) < tolerance);
            phase -= freq;
            if (phase < -std::numbers::pi_v<float>) {
                phase += 2.0f * std::numbers::pi_v<float>;
            }
        }
        const auto out_tags = sink.tags();
        expect(eq(out_tags.size(), tags.size()));
        for (size_t j = 0; j < tags.size(); ++j) {
            expect(tags[j] == out_tags[j]);
        }
    };
};

int main() {}
