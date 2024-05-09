#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <chrono>
#include <numeric>
#include <thread>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const double samp_rate = 100e3;
    auto& source = fg.emplaceBlock<gr::packet_modem::PacketStrobe<int>>(
        25U, std::chrono::seconds(3), "packet_len");
    auto& packet_to_stream = fg.emplaceBlock<gr::packet_modem::PacketToStream<int>>();
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<int>>(samp_rate);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(packet_to_stream)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
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

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
