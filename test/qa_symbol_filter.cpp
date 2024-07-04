#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/symbol_filter.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite SymbolFilterTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "symbol_filter"_test = [] {
        Graph fg;
        const size_t num_symbols = 1000000;
        auto& source =
            fg.emplaceBlock<RandomSource<uint8_t>>({ { "minimum", uint8_t{ 0 } },
                                                     { "maximum", uint8_t{ 1 } },
                                                     { "num_items", num_symbols } });
        const std::vector<float> constellation = { 1.0f, -1.0f };
        auto& constellation_mapper =
            fg.emplaceBlock<Mapper<uint8_t, float>>({ { "map", constellation } });
        const size_t samples_per_symbol = 4U;
        const size_t ntaps = samples_per_symbol * 11U;
        auto rrc_taps = firdes::root_raised_cosine(
            1.0, static_cast<double>(samples_per_symbol), 1.0, 0.35, ntaps);
        auto& rrc_interp = fg.emplaceBlock<InterpolatingFirFilter<float, float, float>>(
            { { "interpolation", samples_per_symbol }, { "taps", rrc_taps } });
        const size_t symbol_filter_pfb_arms = 32UZ;
        const auto rrc_taps_pfb = firdes::root_raised_cosine(
            static_cast<double>(symbol_filter_pfb_arms),
            static_cast<double>(symbol_filter_pfb_arms * samples_per_symbol),
            1.0,
            0.35,
            symbol_filter_pfb_arms * samples_per_symbol * 11U);
        auto& symbol_filter = fg.emplaceBlock<SymbolFilter<float, float, float>>(
            { { "taps", rrc_taps_pfb },
              { "num_arms", symbol_filter_pfb_arms },
              { "samples_per_symbol", samples_per_symbol } });
        auto& sink = fg.emplaceBlock<VectorSink<float>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(constellation_mapper)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(constellation_mapper).to<"in">(rrc_interp)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(rrc_interp).to<"in">(symbol_filter)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(symbol_filter).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_symbols));
        const float tolerance = 5e-3f;
        const float expected_amplitude = 0.24819523f;
        // skip filter transient
        for (auto x : data | std::views::drop(11)) {
            expect(std::abs(std::abs(x) - expected_amplitude) < tolerance);
        }
    };
};

int main() {}
