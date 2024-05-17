#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <ranges>

boost::ut::suite HeadTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

     "head"_test = [] {
        Graph fg;
        constexpr auto num_items = 1000000_ul;
        constexpr auto num_head = 100000_ul;
        std::vector<int> v(static_cast<size_t>(num_items));
        std::iota(v.begin(), v.end(), 0);
        auto& source = fg.emplaceBlock<VectorSource<int>>(v);
        auto& head =
          fg.emplaceBlock<Head<int>>(static_cast<size_t>(num_head));
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink)));
        scheduler::Simple sched{ std::move(fg) };

        // TODO: remove this once https://github.com/fair-acc/gnuradio4/pull/342 is merged
        MsgPortOut toScheduler;
        expect(eq(ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
        std::thread stopper([&toScheduler]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            sendMessage<message::Command::Set>(toScheduler,
                                               "",
                                               block::property::kLifeCycleState,
                                               { { "state", "REQUESTED_STOP" } });
        });
        //
        
        expect(sched.runAndWait().has_value());
        stopper.join();
        const auto sink_data = sink.data();
        const auto r = v | std::views::take(static_cast<size_t>(num_head));
        const std::vector<int> expected(r.begin(), r.end());
        expect(eq(sink.data(), expected));
    };
};

int main() {}
