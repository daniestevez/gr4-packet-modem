#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/message_strobe.hpp>
#include <boost/ut.hpp>
#include <thread>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const gr::property_map message = { { "test", 1 } };
    auto& strobe = fg.emplaceBlock<gr::packet_modem::MessageStrobe<>>(
        { { "message", message }, { "interval_secs", 1.0 } });
    auto& debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS, strobe.strobe.connect(debug.print)));
    expect(eq(gr::ConnectionResult::SUCCESS, strobe.strobe.connect(debug.store)));

    gr::scheduler::Simple sched{ std::move(fg) };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));

    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        fmt::print("sending REQUEST_STOP to scheduler\n");
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    expect(sched.runAndWait().has_value());
    stopper.join();

    fmt::print("MessageDebug stored these messages:\n");
    for (const auto& m : debug.messages()) {
        fmt::print("{}\n", m);
    }

    return 0;
}
