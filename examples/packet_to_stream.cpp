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
        { { "packet_len", uint64_t{ 25 } },
          { "interval_secs", 0.01 },
          { "packet_len_tag_key", "packet_len" },
          { "sleep", false } });
    auto& packet_to_stream = fg.emplaceBlock<gr::packet_modem::PacketToStream<int>>();
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<int>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000U } });
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(packet_to_stream)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });
    const auto ret = sched.runAndWait();
    stopper.join();
    expect(ret.has_value());
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    const auto data = sink.data();
    fmt::print("vector sink contains {} items\n", data.size());
    fmt::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
