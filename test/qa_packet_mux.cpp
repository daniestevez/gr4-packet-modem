#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/packet_mux.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite PacketMuxTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;
    using namespace std::string_literals;

    "packet_mux"_test = [] {
        Graph fg;
        const uint64_t length0 = 100;
        std::vector<int> v0(length0);
        std::iota(v0.begin(), v0.end(), 0);
        const std::vector<gr::Tag> tags0 = {
            { 0, { { "packet_len", length0 }, { "foo", "bar" } } },
            { 27, { { "twenty_seven", 27 } } }
        };
        // use repeat in this one to avoid causing a premature end-of-stream in the
        // mux
        auto& source0 = fg.emplaceBlock<VectorSource<int>>({ { "repeat", true } });
        source0.data = v0;
        source0.tags = tags0;
        const uint64_t length1 = 200;
        std::vector<int> v1(length1);
        std::iota(v1.begin(), v1.end(), 0);
        const std::vector<gr::Tag> tags1 = {
            { 0, { { "packet_len", length1 }, { "baz", 0 } } },
            { 14, { { "fourteen", 14.0 } } }
        };
        auto& source1 = fg.emplaceBlock<VectorSource<int>>();
        source1.data = v1;
        source1.tags = tags1;
        auto& mux = fg.emplaceBlock<PacketMux<int>>({ { "num_inputs", 2UZ } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect(source0, "out"s, mux, "in#0"s))));
        expect(fatal(eq(gr::ConnectionResult::SUCCESS,
                        fg.connect(source1, "out"s, mux, "in#1"s))));
        expect(fatal(
            eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(mux).to<"in">(sink))));

        gr::scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto sink_data = sink.data();
        expect(eq(sink_data.size(), length0 + length1));
        const auto first = sink_data | std::views::take(length0);
        const std::vector<int> first_v(first.begin(), first.end());
        expect(eq(first_v, v0));
        const auto second = sink_data | std::views::drop(length0);
        const std::vector<int> second_v(second.begin(), second.end());
        expect(eq(second_v, v1));
        std::vector<gr::Tag> all_tags = {
            { 0, { { "packet_len", length0 + length1 }, { "foo", "bar" } } },
            { 27, { { "twenty_seven", 27 } } },
            { length0, { { "baz", 0 } } },
            { length0 + 14, { { "fourteen", 14.0 } } }
        };
        expect(sink.tags() == all_tags);
    };

    "pdu_packet_mux"_test = [] {
        Graph fg;
        const Pdu<int> pdu0 = { std::vector{ 1, 2, 3, 4, 5 },
                                { { 0, { { "a", "b" } } }, { 3, { { "c", "d" } } } } };
        const Pdu<int> pdu1 = { std::vector{ 6, 7, 8, 9 }, { { 2, { { "e", "f" } } } } };
        const Pdu<int> pdu2 = { std::vector{ 10, 11, 12, 13, 14, 15 },
                                { { 3, { { "h", "i" } } }, { 5, { { "j", "k" } } } } };
        auto& source0 = fg.emplaceBlock<VectorSource<Pdu<int>>>();
        source0.data = std::vector{ pdu0 };
        auto& source1 = fg.emplaceBlock<VectorSource<Pdu<int>>>();
        source1.data = std::vector{ pdu1 };
        auto& source2 = fg.emplaceBlock<VectorSource<Pdu<int>>>();
        source2.data = std::vector{ pdu2 };
        auto& mux = fg.emplaceBlock<PacketMux<Pdu<int>>>({ { "num_inputs", 3UZ } });
        auto& sink = fg.emplaceBlock<VectorSink<Pdu<int>>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect(source0, "out"s, mux, "in#0"s)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect(source1, "out"s, mux, "in#1"s)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect(source2, "out"s, mux, "in#2"s)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(mux).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        const auto data = sink.data();
        expect(eq(data.size(), 1_u));
        const auto out_pdu = data.at(0);
        const std::vector<int> expected_items = { 1, 2,  3,  4,  5,  6,  7, 8,
                                                  9, 10, 11, 12, 13, 14, 15 };
        const std::vector<Tag> expected_tags = { { 0, { { "a", "b" } } },
                                                 { 3, { { "c", "d" } } },
                                                 { 7, { { "e", "f" } } },
                                                 { 12, { { "h", "i" } } },
                                                 { 14, { { "j", "k" } } } };
        expect(eq(out_pdu.data, expected_items));
        expect(out_pdu.tags == expected_tags);
    };
};

int main() {}
