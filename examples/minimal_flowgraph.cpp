#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <boost/ut.hpp>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::NullSource<int>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::NullSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
