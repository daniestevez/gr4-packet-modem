#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite ItemStrobeTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "item_strobe"_test = [] {
        Graph fg;
        auto& strobe = fg.emplaceBlock<ItemStrobe<int>>({ { "interval_secs", 0.01 } });
        strobe.item = 42;
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(strobe).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        expect(sink.tags().empty());
        const auto data = sink.data();
        // nominally we expect 100 items
        expect(data.size() > 80_ul && data.size() < 120_ul);
        for (const auto x : data) {
            expect(eq(x, 42_i));
        }
    };
};

int main() {}
