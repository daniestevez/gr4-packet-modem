#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

boost::ut::suite ThrottleTests = [] {
    using namespace boost::ut;
    using namespace gr;
    using namespace gr::packet_modem;

    "throttle"_test = [] {
        Graph fg;
        const double samp_rate = 100e3;
        auto& source = fg.emplaceBlock<NullSource<int>>();
        const size_t max_items_per_chunk = 100;
        auto& throttle = fg.emplaceBlock<Throttle<int>>(
            { { "sample_rate", samp_rate },
              { "maximum_items_per_chunk", max_items_per_chunk } });
        auto& sink = fg.emplaceBlock<VectorSink<int>>();
        expect(
            eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(throttle)));
        expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
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
        const auto num_samples = sink.data().size();
        const double expected_num_samples = samp_rate;
        const double ratio = static_cast<double>(num_samples) / expected_num_samples;
        expect(std::abs(ratio - 1.0) < 0.1);
    };
};

int main() {}
