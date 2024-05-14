#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/crc.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
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
                                        { 1, { { "foo", "bar" } } },
                                        { 8, { { "packet_len", 2 } } } };
    // Uncomment just one of these two sources
    //

    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(data, false, tags);

    // auto& source = fg.emplaceBlock<gr::packet_modem::PacketStrobe<uint8_t>>(
    //     8U, std::chrono::seconds(3), "packet_len", true);

    //

    auto& crc_append = fg.emplaceBlock<gr::packet_modem::CrcAppend<>>(
        16U, 0x1021U, 0xFFFFU, 0xFFFFU, true, true);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(crc_append)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(crc_append).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

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
