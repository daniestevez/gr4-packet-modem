#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <boost/ut.hpp>

template <typename T>
struct Source : public gr::Block<Source<T>> {
    gr::PortOut<T> out;
    T d_counter = 0;

    [[nodiscard]] constexpr auto processOne() noexcept
    {
        const auto n = d_counter++;
        fmt::print("source::ProcessOne() = {}\n", n);
        return n;
    }
};

ENABLE_REFLECTION_FOR_TEMPLATE(Source, out);

template <typename T>
struct Sink : public gr::Block<Sink<T>> {
    gr::PortIn<T> in;

    [[nodiscard]] constexpr auto processOne(T a) const noexcept
    {
        fmt::print("sink::ProcessOne({})\n", a);
    }
};

ENABLE_REFLECTION_FOR_TEMPLATE(Sink, in);

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<Source<int>>();
    auto& head = fg.emplaceBlock<gr::packet_modem::Head<int>>(10U);
    auto& sink = fg.emplaceBlock<Sink<int>>();
    auto& vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(vector_sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());
}
