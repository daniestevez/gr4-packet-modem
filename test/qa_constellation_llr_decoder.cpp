#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/constellation_llr_decoder.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite ConstellationLLRDecoderTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "constellation_llr_decoder"_test = [](auto constellation) {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        using c64 = std::complex<float>;
        auto& source = fg.emplaceBlock<NoiseSource<c64>>();
        auto& head = fg.emplaceBlock<Head<c64>>(
            { { "num_items", static_cast<size_t>(num_items) } });
        auto& constellation_decoder = fg.emplaceBlock<ConstellationLLRDecoder<>>(
            { { "constellation", constellation } });
        auto& sink_source = fg.emplaceBlock<VectorSink<c64>>();
        auto& sink = fg.emplaceBlock<VectorSink<float>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(head).to<"in">(constellation_decoder)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink_source)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(constellation_decoder, "out"s, sink, "in"s)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data_source = sink_source.data();
        const auto data = sink.data();
        expect(eq(data_source.size(), num_items));
        const float scale = 2.0f; // see calculation in ConstellationLLRDecoder
        if (constellation == "BPSK") {
            expect(eq(data.size(), num_items));
            for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
                expect(eq(data[j], scale * data_source[j].real()));
            }
        } else {
            // QPSK
            expect(eq(data.size(), 2UZ * static_cast<size_t>(num_items)));
            for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
                expect(eq(data[2 * j], scale * data_source[j].real()));
                expect(eq(data[2 * j + 1], scale * data_source[j].imag()));
            }
        }
        expect(sink_source.tags().empty());
        expect(sink.tags().empty());
    } | std::vector<std::string>{ "BPSK", "QPSK" };
};

int main() {}
