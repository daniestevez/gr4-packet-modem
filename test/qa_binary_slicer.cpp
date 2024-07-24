#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/binary_slicer.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite SlicerTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "slicer"_test = []<typename Invert> {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source = fg.emplaceBlock<NoiseSource<float>>();
        auto& head = fg.emplaceBlock<Head<float>>(
            { { "num_items", static_cast<size_t>(num_items) } });
        auto& slicer = fg.emplaceBlock<BinarySlicer<Invert::value>>();
        auto& sink_source = fg.emplaceBlock<VectorSink<float>>();
        auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(slicer)));
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink_source)));
        // For some reason, using the notation
        // fg.connect<"out">(slicer).to<"in">(sink) gives a compile error when
        // BinarySlicer depends on Invert::value.
        expect(eq(ConnectionResult::SUCCESS, fg.connect(slicer, "out"s, sink, "in"s)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data_source = sink_source.data();
        const auto data = sink.data();
        expect(eq(data_source.size(), num_items));
        expect(eq(data.size(), num_items));
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            if constexpr (Invert::value) {
                expect(eq(data[j], data_source[j] < 0.0f ? uint8_t{ 1 } : uint8_t{ 0 }));
            } else {
                expect(eq(data[j], data_source[j] > 0.0f ? uint8_t{ 1 } : uint8_t{ 0 }));
            }
        }
        expect(sink_source.tags().empty());
        expect(sink.tags().empty());
    } | std::tuple<std::false_type, std::true_type>{};
};

int main() {}
