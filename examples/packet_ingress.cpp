#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_ingress.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <thread>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const std::vector<size_t> packet_lengths = { 10,     23, 2,     1024, 7,
                                                 100000, 45, 51234, 28,   100 };
    size_t offset = 0;
    std::vector<gr::Tag> tags;
    for (const auto len : packet_lengths) {
        gr::property_map props = { { "packet_len", len } };
        if (len > std::numeric_limits<uint16_t>::max()) {
            props["invalid"] = true;
        }
        tags.push_back({ static_cast<ssize_t>(offset), props });
        if (len > 50) {
            tags.push_back(
                { static_cast<ssize_t>(offset + 40), { { "packet_rest", true } } });
        }
        offset += len;
    }
    std::vector<uint8_t> v(offset);
    auto& packet_source = fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>();
    packet_source.data = v;
    packet_source.tags = tags;

    auto& ingress = fg.emplaceBlock<gr::packet_modem::PacketIngress<>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    auto& msg_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS, ingress.metadata.connect(msg_debug.print)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_source).to<"in">(ingress)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(ingress).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));

    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        fmt::print("sending REQUEST_STOP to scheduler\n");
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    expect(sched.runAndWait().has_value());
    stopper.join();

    const auto data = sink.data();
    fmt::print("vector sink contains {} items\n", data.size());
    fmt::print("vector sink tags:\n");
    const auto sink_tags = sink.tags();
    for (const auto& t : sink_tags) {
        fmt::print("index = {}, map = {}\n", t.index, t.map);
    }

    return 0;
}
