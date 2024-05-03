#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
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
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(head)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    fmt::print("calling sched.runAndWait()");
    expect(sched.runAndWait().has_value());
    fmt::print("sched.runAndWait() has returned");
}
