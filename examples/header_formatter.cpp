#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/message_strobe.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>
#include <thread>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const gr::property_map message = { { "packet_length", 1234 } };
    auto& strobe = fg.emplaceBlock<gr::packet_modem::MessageStrobe<>>(
        { { "message", message }, { "interval_secs", 0.1 } });
    auto& header_formatter = fg.emplaceBlock<gr::packet_modem::HeaderFormatter>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              strobe.strobe.connect(header_formatter.metadata)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(header_formatter).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));

    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        fmt::print("sending REQUEST_STOP to scheduler\n");
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    expect(sched.runAndWait().has_value());
    stopper.join();

    const auto data = sink.data();
    fmt::print("vector sink contains {} items\n", data.size());
    fmt::print("vector sink items:\n");
    for (const auto n : data) {
        fmt::print("{} ", n);
    }
    fmt::print("\n");
    fmt::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
