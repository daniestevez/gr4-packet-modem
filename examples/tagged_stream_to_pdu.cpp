#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <numeric>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    std::vector<int> v(30);
    std::iota(v.begin(), v.end(), 0);
    const std::vector<gr::Tag> tags = { { 0, { { "packet_len", 10 } } },
                                        { 3, { { "foo", "bar" } } },
                                        { 10, { { "packet_len", 20 } } } };
    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<int>>();
    source.data = v;
    source.tags = tags;
    auto& stream_to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<int>>();
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::VectorSink<gr::packet_modem::Pdu<int>>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(stream_to_pdu)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(stream_to_pdu).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    const auto data = sink.data();
    fmt::print("vector sink contains {} items\n", data.size());
    fmt::print("vector sink items:\n");
    for (const auto& pdu : data) {
        fmt::println("data = {}", pdu.data);
        fmt::println("tags:");
        for (const auto& t : pdu.tags) {
            fmt::println("index = {}, map = {}", t.index, t.map);
        }
    }
    fmt::print("\n");
    fmt::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
