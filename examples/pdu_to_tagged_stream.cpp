#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
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

    std::vector<gr::packet_modem::Pdu<int>> pdus = {
        { { 1, 2, 3, 4, 5 }, { { 0, { { "foo", "bar" }, { "baz", 7 } } } } },
        { { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20 }, { { 3, { { "a", "b" } } } } }
    };
    pdus.emplace_back(std::vector<int>(100000));
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<int>>>(pdus);
    auto& pdu_to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<int>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(pdu_to_stream)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(pdu_to_stream).to<"in">(sink)));

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
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
