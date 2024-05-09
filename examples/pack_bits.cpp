#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const std::vector<uint8_t> v = { 0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0 };
    // This works as expected
    const std::vector<gr::Tag> tags = {
        { 0, { { "zero", pmtv::pmt_null() } } },
        { 2, { { "two", pmtv::pmt_null() } } },
    };
    // This makes the scheduler loop forever. PackBits::processBulk() is never called
    // const std::vector<gr::Tag> tags = {
    //     { 1, { { "one", pmtv::pmt_null() } } },
    // };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(v, false, tags);
    auto& pack = fg.emplaceBlock<gr::packet_modem::PackBits<>>(2U);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(pack).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink items:\n");
    for (const auto n : data) {
        std::print("{} ", n);
    }
    std::print("\n");
    std::print("vector sink tags:\n");
    for (const auto& t : sink.tags()) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
