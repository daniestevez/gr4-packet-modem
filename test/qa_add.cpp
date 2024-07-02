#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite AddTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "add"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source0 = fg.emplaceBlock<RandomSource<int>>(
            { { "minimum", -1000 },
              { "maximum", 1000 },
              { "num_items", static_cast<size_t>(num_items) } });
        auto& source1 = fg.emplaceBlock<RandomSource<int>>(
            { { "minimum", -1000 },
              { "maximum", 1000 },
              { "num_items", static_cast<size_t>(num_items) } });
        auto& add = fg.emplaceBlock<Add<int>>();
        auto& sink_source0 = fg.emplaceBlock<VectorSink<int>>();
        auto& sink_source1 = fg.emplaceBlock<VectorSink<int>>();
        auto& sink_add = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source0).to<"in0">(add)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source1).to<"in1">(add)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source0).to<"in">(sink_source0)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source1).to<"in">(sink_source1)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(add).to<"in">(sink_add)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data0 = sink_source0.data();
        const auto data1 = sink_source1.data();
        const auto data_add = sink_add.data();
        expect(eq(data0.size(), num_items));
        expect(eq(data1.size(), num_items));
        expect(eq(data_add.size(), num_items));
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            expect(eq(data0[j] + data1[j], data_add[j]));
        }
        expect(sink_source0.tags().empty());
        expect(sink_source1.tags().empty());
        expect(sink_add.tags().empty());
    };
};

int main() {}
