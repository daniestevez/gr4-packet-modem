#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    const auto v = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    auto& vector_source = fg.emplaceBlock<gr::packet_modem::VectorSource<int>>(v, true);
    auto& head = fg.emplaceBlock<gr::packet_modem::Head<int>>(1000000U);
    auto& vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(vector_source).to<"in">(head)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(vector_sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    const auto data = vector_sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink contents:\n");
    for (const auto n : data) {
        std::print("{} ", n);
    }
    std::print("\n");
}
