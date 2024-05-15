#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/crc.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <numeric>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::RandomSource<uint8_t>>(
        uint8_t{ 0 }, uint8_t{ 255 }, 1000U, false);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };

    // gr::MsgPortOut toScheduler;
    // expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    // std::thread stopper([&toScheduler]() {
    //     std::this_thread::sleep_for(std::chrono::milliseconds(100));
    //     gr::sendMessage<gr::message::Command::Set>(toScheduler,
    //                                                "",
    //                                                gr::block::property::kLifeCycleState,
    //                                                { { "state", "REQUESTED_STOP" } });
    // });

    const auto ret = sched.runAndWait();
    // stopper.join();
    if (!ret.has_value()) {
        std::print("ret = {}\n", ret.error());
    }
    expect(ret.has_value());

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink items:\n");
    for (const auto x : data) {
        std::print("{} ", x);
    }
    std::print("\n");
    std::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
