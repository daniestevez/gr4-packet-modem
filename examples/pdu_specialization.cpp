#include <fmt/core.h>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <boost/ut.hpp>

template <typename T>
class Process : public gr::Block<Process<T>>
{
public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    T gain = T{ 1 };

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        fmt::println("Process<T>::processBulk(inSpan.size(), outSpan.size() = {})",
                     inSpan.size(),
                     outSpan.size());
        const auto n = std::min(inSpan.size(), outSpan.size());
        for (size_t j = 0; j < n; ++j) {
            outSpan[j] = inSpan[j] * gain;
        }
        std::ignore = inSpan.consume(n);
        outSpan.publish(n);
        return gr::work::Status::OK;
    }
};

template <typename T>
class Process<gr::packet_modem::Pdu<T>>
    : public gr::Block<Process<gr::packet_modem::Pdu<T>>>
{
public:
    gr::PortIn<gr::packet_modem::Pdu<T>> in;
    gr::PortOut<gr::packet_modem::Pdu<T>> out;

    T gain = T{ 1 };

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        fmt::println("Process<Pdu<T>>::processBulk(inSpan.size(), outSpan.size() = {})",
                     inSpan.size(),
                     outSpan.size());
        const auto n = std::min(inSpan.size(), outSpan.size());
        for (size_t j = 0; j < n; ++j) {
            outSpan[j].data.clear();
            outSpan[j].data.reserve(inSpan[j].data.size());
            for (const auto a : inSpan[j].data) {
                outSpan[j].data.push_back(a * gain);
            }
            outSpan[j].tags = inSpan[j].tags;
        }
        std::ignore = inSpan.consume(n);
        outSpan.publish(n);
        return gr::work::Status::OK;
    }
};

ENABLE_REFLECTION_FOR_TEMPLATE(Process, in, out);

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    const gr::packet_modem::Pdu<int> pdu(std::vector{ 1, 2, 3, 4, 5, 6 }, {});
    const std::vector<gr::packet_modem::Pdu<int>> pdus = { pdu };
    auto& source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<int>>>(pdus);
    auto& to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<int>>();
    auto& process = fg.emplaceBlock<Process<int>>();
    auto& process_pdu = fg.emplaceBlock<Process<gr::packet_modem::Pdu<int>>>();
    process.gain = 3;
    process_pdu.gain = 4;
    auto& to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<int>>();
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::VectorSink<gr::packet_modem::Pdu<int>>>();
    auto& sink_pdu =
        fg.emplaceBlock<gr::packet_modem::VectorSink<gr::packet_modem::Pdu<int>>>();
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(to_stream)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(to_stream).to<"in">(process)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(process).to<"in">(to_pdu)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(to_pdu).to<"in">(sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(source).to<"in">(process_pdu)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(process_pdu).to<"in">(sink_pdu)));
    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    for (const auto& p : sink.data()) {
        fmt::println("pdu.data = {}", p.data);
    }

    for (const auto& p : sink_pdu.data()) {
        fmt::println("pdu.data = {}", p.data);
    }

    return 0;
}
