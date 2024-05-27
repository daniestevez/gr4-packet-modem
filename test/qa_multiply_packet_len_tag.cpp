#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/multiply_packet_len_tag.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite MultiplyPacketLenTagTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "multiply_packet_len_tag"_test = [] {
        Graph fg;
        const std::vector<Tag> tags = { { 0, { { "packet_len", 7UZ } } },
                                        { 7, { { "packet_len", 23UZ } } } };
        std::vector<int> v(30);
        std::iota(v.begin(), v.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>(v, false, tags);
        auto& fec = fg.emplaceBlock<MultiplyPacketLenTag<int>>(5.0);
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(fec)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(fec).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(sink.data(), v));
        const std::vector<Tag> expected_tags = { { 0, { { "packet_len", 35UZ } } },
                                                 { 7, { { "packet_len", 115UZ } } } };
        expect(sink.tags() == expected_tags);
    };
};

int main() {}
