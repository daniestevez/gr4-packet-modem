#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/tag_gate.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite VectorSourceAndSinkTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "tag_gate"_test = [] {
        Graph fg;
        std::vector<int> v(100);
        std::iota(v.begin(), v.end(), 0);
        const std::vector<Tag> tags = {
            { 0, { { "a", pmtv::pmt_null() } } },
            { 10, { { "b", 0.1234 }, { "c", 12345U } } },
            { 73, { { "d", std::vector<int>{ 1, 2, 3 } }, { "e", 0.0f } } },
            { std::ssize(v) - 1, { { "f", pmtv::pmt_null() } } }
        };
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        source.data = v;
        source.tags = tags;
        auto& tag_gate = fg.emplaceBlock<TagGate<int>>();
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(tag_gate)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(tag_gate).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), v));
        expect(sink.tags().empty());
    };
};

int main() {}
