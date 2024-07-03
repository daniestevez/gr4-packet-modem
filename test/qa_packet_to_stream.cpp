#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite PacketToStreamTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "packet_to_stream"_test = [] {
        Graph fg;
        const double samp_rate = 10e3;
        const size_t packet_len = 25;
        auto& source = fg.emplaceBlock<ItemStrobe<Pdu<int>>>(
            { { "interval_secs", 0.1 }, { "sleep", false } });
        source.item = Pdu<int>{ std::vector<int>(packet_len, 1), {} };
        auto& pdu_to_tagged = fg.emplaceBlock<PduToTaggedStream<int>>();
        auto& packet_to_stream = fg.emplaceBlock<PacketToStream<int>>();
        packet_to_stream.out.max_samples = 100U;
        auto& throttle = fg.emplaceBlock<Throttle<int>>(
            { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 100UZ } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(pdu_to_tagged)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_tagged).to<"in">(packet_to_stream)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto data = sink.data();
        // nominally we expect 100000 samples
        expect(data.size() > 40000_ul);
        expect(data.size() < 120000_ul);
        int sum = 0;
        for (auto x : data) {
            expect(x == 0 || x == 1);
            sum += x;
        }
        int num_packets = sum / static_cast<int>(packet_len);
        // nominally we expect 50 packets
        expect(num_packets > 40_i);
        expect(num_packets < 60_i);
    };

    "packet_to_stream_pdu"_test = [] {
        Graph fg;
        const double samp_rate = 10e3;
        const size_t packet_len = 25;
        auto& source = fg.emplaceBlock<ItemStrobe<Pdu<int>>>(
            { { "interval_secs", 0.1 }, { "sleep", false } });
        source.item = Pdu<int>{ std::vector<int>(packet_len, 1), {} };
        auto& packet_to_stream = fg.emplaceBlock<PacketToStream<Pdu<int>>>();
        packet_to_stream.out.max_samples = 100U;
        auto& throttle = fg.emplaceBlock<Throttle<int>>(
            { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 100UZ } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(packet_to_stream)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto data = sink.data();
        // nominally we expect 100000 samples
        expect(data.size() > 40000_ul);
        expect(data.size() < 120000_ul);
        int sum = 0;
        for (auto x : data) {
            expect(x == 0 || x == 1);
            sum += x;
        }
        int num_packets = sum / static_cast<int>(packet_len);
        // nominally we expect 50 packets
        expect(num_packets > 40_i);
        expect(num_packets < 60_i);
    };
};

int main() {}
