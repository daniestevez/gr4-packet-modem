#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <numeric>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    std::vector<uint8_t> data(10);
    const std::vector<gr::Tag> tags = { { 0, { { "packet_len", 8 } } },
                                        //{ 1, { { "foo", "bar" } } },
                                        { 8, { { "packet_len", 2 } } } };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(data, false, tags);
    auto& crc_append = fg.emplaceBlock<gr::packet_modem::CrcAppend<>>(
        16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
    auto& crc_check = fg.emplaceBlock<gr::packet_modem::CrcCheck<>>(
        16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(crc_append)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(crc_append).to<"in">(crc_check)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(crc_check).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };

    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    const auto ret = sched.runAndWait();
    stopper.join();
    if (!ret.has_value()) {
        std::print("ret = {}\n", ret.error());
    }
    expect(ret.has_value());

    const auto sink_data = sink.data();
    std::print("vector sink contains {} items\n", sink_data.size());
    std::print("vector sink items:\n");
    for (const auto n : sink_data) {
        std::print("0x{:02x} ", n);
    }
    std::print("\n");
    std::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
