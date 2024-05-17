#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite PduTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "pdu_to_tagged_stream_fixed"_test = [] {
        Graph fg;
        std::vector<Pdu<int>> pdus = { { { 1, 2, 3, 4, 5 },
                                         { { 0, { { "foo", "bar" }, { "baz", 7 } } } } },
                                       { { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 },
                                         { { 3, { { "a", "b" } } } } } };
        auto& source = fg.emplaceBlock<VectorSource<Pdu<int>>>(pdus);
        auto& pdu_to_stream = fg.emplaceBlock<PduToTaggedStream<int>>();
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(pdu_to_stream)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_stream).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const std::vector<int> expected_data = { 1,  2,  3,  4,  5,  10, 11, 12,
                                                 13, 14, 15, 16, 17, 18, 19, 20 };
        expect(eq(sink.data(), expected_data));
        const std::vector<Tag> expected_tags = {
            { 0, { { "foo", "bar" }, { "baz", 7 }, { "packet_len", 5UZ } } },
            { 5, { { "packet_len", 11UZ } } },
            { 8, { { "a", "b" } } }
        };
        expect(sink.tags() == expected_tags);
    };

    "tagged_stream_to_pdu_fixed"_test = [] {
        Graph fg;
        std::vector<int> v(30);
        std::iota(v.begin(), v.end(), 0);
        const std::vector<Tag> tags = { { 0, { { "packet_len", 10 } } },
                                        { 3, { { "foo", "bar" } } },
                                        { 10, { { "packet_len", 20 } } } };
        auto& source = fg.emplaceBlock<VectorSource<int>>(v, false, tags);
        auto& stream_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<int>>();
        auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<Pdu<int>>>();
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(stream_to_pdu)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(stream_to_pdu).to<"in">(sink)));
        gr::scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto pdus = sink.data();
        expect(sink.tags().empty());
        expect(eq(pdus.size(), 2_u));
        const std::vector<int> expected_0(v.cbegin(), v.cbegin() + 10);
        const std::vector<int> expected_1(v.cbegin() + 10, v.cend());
        expect(eq(pdus[0].data, expected_0));
        expect(eq(pdus[1].data, expected_1));
        const std::vector<Tag> tags_0 = { { 3, { { "foo", "bar" } } } };
        expect(pdus[0].tags == tags_0);
        expect(pdus[1].tags.empty());
    };
};

int main() {}
