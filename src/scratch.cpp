#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
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

    const uint64_t source_len = 25U;
    const std::vector<gr::Tag> source_tags = { { 0, { { "packet_len", source_len } } } };
    std::vector<uint8_t> source_data(source_len);
    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(
        source_data, true, source_tags);
    auto& scrambler = fg.emplaceBlock<gr::packet_modem::AdditiveScrambler<uint8_t>>(
        0x21U, 0x1ffU, 8U, 0U, "packet_len");
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(scrambler)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(scrambler).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));

    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });
    expect(sched.runAndWait().has_value());
    stopper.join();

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink items:\n");
    for (const auto n : data) {
        std::print("{} ", n);
    }
    std::print("\n");
    std::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
