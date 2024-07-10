#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <boost/ut.hpp>

using namespace gr;

class Source : public Block<Source>
{
public:
    PortOut<int, RequiredSamples<1, 16>, StreamBufferType<CircularBuffer<int, 32>>> out;

    work::Status processBulk(PublishableSpan auto& outSpan)
    {
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, outSpan.size());
    }
};

ENABLE_REFLECTION(Source, out);

class Sink : public Block<Sink>
{
public:
    PortIn<int, RequiredSamples<1, 16>, StreamBufferType<CircularBuffer<int, 32>>> in;

    work::Status processBulk(const ConsumableSpan auto& inSpan)
    {
        fmt::println("{}::processBulk(inSpan.size() = {})", this->name, inSpan.size());
    }
};

ENABLE_REFLECTION(Sink, in);

int main()
{
    using namespace boost::ut;

    Graph fg;
    auto& source = fg.emplaceBlock<Source>();
    // source.out.resizeBuffer(32);
    auto& sink = fg.emplaceBlock<Sink>();
    // sink.in.resizeBuffer(32);
    expect(eq(ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
