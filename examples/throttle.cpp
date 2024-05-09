#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

int main()
{
    using namespace boost::ut;

    const double samp_rate = 1e6;
    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::NullSource<int>>();
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<int>>(samp_rate);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };

    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });
    expect(sched.runAndWait().has_value());
    stopper.join();

    std::print("vector sink contains {} items\n", sink.data().size());

    return 0;
}
