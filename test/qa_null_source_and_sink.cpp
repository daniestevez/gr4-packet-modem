#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

boost::ut::suite NullSourceAndSinkTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "null_source"_test = [] {
        Graph fg;
        auto& source = fg.emplaceBlock<NullSource<int>>();
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        expect(sched.runAndWait().has_value());
        stopper.join();
        expect(gt(sink.data().size(), 0_ul));
        for (const auto x : sink.data()) {
            expect(eq(x, 0_i));
        }
    };

    "null_sink"_test = [] {
        Graph fg;
        constexpr auto num_items = 100000_ul;
        std::vector<int> items(static_cast<size_t>(num_items));
        std::iota(items.begin(), items.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>();
        source.data = items;
        auto& null_sink = fg.emplaceBlock<NullSink<int>>();
        auto& vector_sink = fg.emplaceBlock<VectorSink<int>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(null_sink)));
        expect(eq(ConnectionResult::SUCCESS,
                  fg.connect<"out">(source).to<"in">(vector_sink)));
        scheduler::Simple sched{ std::move(fg) };
        expect(sched.runAndWait().has_value());
        expect(eq(vector_sink.data().size(), num_items));
    };
};

int main() {}
