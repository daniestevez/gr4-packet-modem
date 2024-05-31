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
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(source0, { "out" }, mux, { "in", 0 })));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(source1, { "out" }, mux, { "in", 1 })));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect(source2, { "out" }, mux, { "in", 2 })));
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
