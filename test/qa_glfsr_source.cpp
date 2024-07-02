#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/glfsr_source.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite GlfsrSourceTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "glfsr_source"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        auto& source = fg.emplaceBlock<GlfsrSource<>>();
        auto& head = fg.emplaceBlock<Head<uint8_t>>(
            { { "num_items", static_cast<size_t>(num_items) } });
        auto& sink = fg.emplaceBlock<VectorSink<uint8_t>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), num_items));
        size_t sum = 0;
        for (size_t j = 0; j < static_cast<size_t>(num_items); ++j) {
            expect(data[j] == 0 || data[j] == 1);
            sum += static_cast<size_t>(data[j]);
        }
        // we expect a somewhat balanced output
        expect(sum > 40000_ul && sum < 60000_ul);
        expect(sink.tags().empty());
    };
};

int main() {}
