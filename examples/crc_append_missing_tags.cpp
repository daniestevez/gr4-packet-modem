#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <cstdint>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    const size_t packet_length = 1500;
    const std::vector<uint8_t> v(packet_length);
    const std::vector<gr::Tag> tags = { { 0, { { "packet_len", packet_length } } } };
    auto& vector_source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(v, true, tags);
    auto& crc_append = fg.emplaceBlock<gr::packet_modem::CrcAppend<>>(
        32U, 0x4C11DB7U, 0xFFFFFFFFU, 0xFFFFFFFFU, true, true);
    auto& null_sink = fg.emplaceBlock<gr::packet_modem::NullSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(vector_source).to<"in">(crc_append)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(crc_append).to<"in">(null_sink)));
    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded> sched{ std::move(
        fg) };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
