#include <fmt/core.h>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <boost/ut.hpp>

class Source : public gr::Block<Source>
{
public:
    gr::PortOut<int> out;
    gr::MsgPortOut msg_out;

    size_t d_remaining = 1000000U;

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
        fmt::println("Source::processBulk(outSpan.size() = {}), d_remaining = {}",
                     outSpan.size(),
                     d_remaining);
        gr::sendMessage<gr::message::Command::Invalid>(
            msg_out, "", "", gr::property_map{});
        const auto n = std::min(d_remaining, outSpan.size());
        outSpan.publish(n);
        fmt::println("Source published {}", n);
        d_remaining -= n;
        return d_remaining == 0 ? gr::work::Status::DONE : gr::work::Status::OK;
    }
};

ENABLE_REFLECTION(Source, out, msg_out);

class ZeroSource : public gr::Block<ZeroSource>
{
public:
    gr::PortOut<int> out;

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
        fmt::println("ZeroSource::processBulk(outSpan.size() = {})", outSpan.size());
        outSpan.publish(0);
        return gr::work::Status::DONE;
    }
};

ENABLE_REFLECTION(ZeroSource, out);

class Sink : public gr::Block<Sink>
{
public:
    gr::PortIn<int> in;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan)
    {
        fmt::println("Sink::processBulk(inSpan.size() = {})", inSpan.size());
        std::ignore = inSpan.consume(inSpan.size());
        return gr::work::Status::OK;
    }
};

ENABLE_REFLECTION(Sink, in);

class MessageSink : public gr::Block<MessageSink>
{
public:
    gr::MsgPortInNamed<"msg_in"> msg_in;
    gr::PortIn<int> fake_in; // needed because otherwise the block doesn't compile

    void processMessages(gr::MsgPortInNamed<"msg_in">&,
                         std::span<const gr::Message> messages)
    {
        fmt::println("MessageSink::processMessages(messages.size() = {})",
                     messages.size());
    }

    void processOne(int) { return; }
};

ENABLE_REFLECTION(MessageSink, fake_in, msg_in);

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<Source>();
    auto& zero_source = fg.emplaceBlock<ZeroSource>();
    auto& sink = fg.emplaceBlock<Sink>();
    auto& msg_sink = fg.emplaceBlock<MessageSink>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(zero_source).to<"fake_in">(msg_sink)));
    expect(eq(gr::ConnectionResult::SUCCESS, source.msg_out.connect(msg_sink.msg_in)));
    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
