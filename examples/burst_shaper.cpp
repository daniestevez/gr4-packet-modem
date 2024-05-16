#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/burst_shaper.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <numeric>

int main()
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    gr::Graph fg;

    const std::vector<c64> v(30, { 1.0f, -1.0f });
    const std::vector<gr::Tag> tags = { { 0, { { "packet_len", 10 } } },
                                        { 10, { { "packet_len", 20 } } } };
    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<c64>>(v, false, tags);

    const std::vector<float> leading_shape = { 0.2f, 0.4f, 0.6f, 0.8f, 0.9f };
    const std::vector<float> trailing_shape = { 0.8f, 0.5f, 0.1f };
    auto& shaper = fg.emplaceBlock<gr::packet_modem::BurstShaper<c64, c64, float>>(
        leading_shape, trailing_shape);

    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<c64>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(shaper)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(shaper).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink items:\n");
    for (const auto n : data) {
        fmt::print("{} ", n);
    }
    std::print("\n");
    std::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
