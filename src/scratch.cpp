#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <numeric>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    std::vector<int> v(100);
    std::iota(v.begin(), v.end(), 0);

    const std::vector<gr::Tag> tags = {
        { 0, { { "begin", pmtv::pmt_null() } } },
        { 10, { { "param_a", 3.1415 }, { "param_b", 12345U } } },
        { 73, { { "param_c", std::vector<int>{ 1, 2, 3 } }, { "param_d", 0.0f } } },
        { std::ssize(v) - 1, { { "end", pmtv::pmt_null() } } }
    };

    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<int>>(v, false, tags);
    auto& sink = fg.emplaceBlock<gr::packet_modem::FileSink<int>>("/tmp/test_file_sink");
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
