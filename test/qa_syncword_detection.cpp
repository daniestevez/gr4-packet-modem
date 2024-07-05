#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <numbers>
#include <random>

boost::ut::suite SyncwordDetectionTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "syncword_detection"_test = [](float freq_error) {
        Graph fg;
        using c64 = std::complex<float>;
        const size_t num_symbols = 1000000;
        std::vector<uint8_t> symbols(num_symbols);
        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<uint8_t> dist(0, 1);
        for (auto& symbol : symbols) {
            symbol = dist(e);
        }
        const std::vector<size_t> syncword_locations = { 100,    1000,  1250,  10000,
                                                         13721,  43124, 58000, 127018,
                                                         525178, 893251 };
        const std::vector<uint8_t> syncword = {
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 },
            uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
            uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
            uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
            uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
            uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
            uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
            uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }
        };
        for (auto loc : syncword_locations) {
            for (size_t j = 0; j < syncword.size(); ++j) {
                symbols[loc + j] = syncword[j];
            }
        }
        auto& source = fg.emplaceBlock<VectorSource<uint8_t>>();
        source.data = symbols;
        const std::vector<c64> constellation = { { 1.0f, 0.0f }, { -1.0f, 0.0f } };
        auto& constellation_mapper =
            fg.emplaceBlock<Mapper<uint8_t, c64>>({ { "map", constellation } });
        const size_t samples_per_symbol = 4U;
        const size_t ntaps = samples_per_symbol * 11U;
        auto rrc_taps = firdes::root_raised_cosine(
            1.0, static_cast<double>(samples_per_symbol), 1.0, 0.35, ntaps);
        // normalize RRC taps to unity RMS norm
        float rrc_taps_norm = 0.0f;
        for (auto x : rrc_taps) {
            rrc_taps_norm += x * x;
        }
        rrc_taps_norm = std::sqrt(rrc_taps_norm);
        for (auto& x : rrc_taps) {
            x /= rrc_taps_norm;
        }
        auto& rrc_interp = fg.emplaceBlock<InterpolatingFirFilter<c64, c64, float>>(
            { { "interpolation", samples_per_symbol }, { "taps", rrc_taps } });
        auto& rotator = fg.emplaceBlock<gr::packet_modem::Rotator<>>(
            { { "phase_incr", freq_error } });
        auto& syncword_detection = fg.emplaceBlock<SyncwordDetection>(
            { { "rrc_taps", rrc_taps },
              { "syncword", syncword },
              { "constellation", constellation },
              { "min_freq_bin", -4 },
              { "max_freq_bin", 4 },
              // set a high power threshold to avoid false detections
              { "power_threshold", 20.0f } });
        auto& sink_input = fg.emplaceBlock<VectorSink<c64>>();
        auto& sink_output = fg.emplaceBlock<VectorSink<c64>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(constellation_mapper)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(constellation_mapper).to<"in">(rrc_interp)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(rrc_interp).to<"in">(rotator)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(rotator).to<"in">(syncword_detection)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(rotator).to<"in">(sink_input)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(syncword_detection).to<"in">(sink_output)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const size_t delay =
            2 * static_cast<size_t>(syncword_detection.time_threshold) + 1;
        const auto data_in = sink_input.data();
        expect(eq(data_in.size(), samples_per_symbol * num_symbols));
        const auto data_out = sink_output.data();
        expect(data_out.size() <= data_in.size());
        expect(data_out.size() + syncword_detection.fft_size > data_in.size());
        for (size_t j = 0; j < delay; ++j) {
            expect(eq(data_out[j], c64{ 0.0f, 0.0f }));
        }
        for (size_t j = delay; j < data_out.size(); ++j) {
            expect(eq(data_out[j], data_in[j - delay]));
        }
        const auto tags = sink_output.tags();
        expect(eq(tags.size(), syncword_locations.size()));
        for (size_t j = 0; j < tags.size(); ++j) {
            const auto& tag = tags[j];
            const size_t expected_index =
                delay + samples_per_symbol * syncword_locations[j];
            expect(eq(static_cast<size_t>(tag.index), expected_index));
            const auto& meta = tag.map;
            const auto syncword_amplitude =
                pmtv::cast<float>(meta.at("syncword_amplitude"));
            expect(syncword_amplitude < 1.01f);
            expect(syncword_amplitude > 0.95f);
            const auto syncword_esn0_db = pmtv::cast<float>(meta.at("syncword_esn0_db"));
            expect(syncword_esn0_db >= 30.0f);
            const auto syncword_freq = pmtv::cast<float>(meta.at("syncword_freq"));
            expect(std::abs(syncword_freq - freq_error) < 5e-4f);
            const auto syncword_freq_bin = pmtv::cast<int>(meta.at("syncword_freq_bin"));
            const double bin_spacing =
                std::numbers::pi /
                static_cast<double>(syncword_detection._syncword_samples_size);
            const int expected_bin = static_cast<int>(
                std::round(static_cast<double>(freq_error) / bin_spacing));
            expect(eq(syncword_freq_bin, expected_bin));
            const auto syncword_noise_power =
                pmtv::cast<float>(meta.at("syncword_noise_power"));
            expect(syncword_noise_power < 3e-4f);
            const auto syncword_phase = pmtv::cast<float>(meta.at("syncword_phase"));
            if (freq_error == 0.0f) {
                expect(std::abs(syncword_phase) < 1e-6f);
            }
            const auto syncword_time_est =
                pmtv::cast<float>(meta.at("syncword_time_est"));
            expect(std::abs(syncword_time_est) < 0.05f);
        }
        for (const auto& tag : tags) {
            fmt::println("tag {} {}", tag.index, tag.map);
        }
    } | std::vector<float>{ 0.0f, 0.005f, 0.015f, -0.005f, -0.015f };
};

int main() {}
