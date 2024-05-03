#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <boost/ut.hpp>

template <typename T>
struct Source : public gr::Block<Source<T>> {
    gr::PortOut<T> out;

    [[nodiscard]] constexpr auto processOne() const noexcept
    {
        fmt::print("source::ProcessOne()\n");
        return 0;
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
    auto& sink = fg.emplaceBlock<Sink<int>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());
}
