#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_counter.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <cstdint>
#include <cstdlib>

class PacketSource : public gr::Block<PacketSource>
{
public:
    gr::PortIn<gr::Message, gr::Async> count;
    gr::PortOut<gr::packet_modem::Pdu<uint8_t>, gr::RequiredSamples<1U, 1U>> out;
    static constexpr size_t packet_size = 256UZ;
    static constexpr size_t max_packets = 2UZ;
    uint64_t _exit_count;
    uint64_t _entry_count;

    void start()
    {
        _entry_count = 0;
        _exit_count = 0;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& countSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(countSpan.size() = {}, outSpan.size() = {}), "
                     "_entry_count = {}, _exit_count = {}",
                     this->name,
                     countSpan.size(),
                     outSpan.size(),
                     _entry_count,
                     _exit_count);
#endif

        if (countSpan.size() > 0) {
            const auto& msg = countSpan[countSpan.size() - 1];
            _exit_count = pmtv::cast<uint64_t>(msg.data.value().at("packet_count"));
#ifdef TRACE
            fmt::println("{} updated _exit_count = {}", this->name, _exit_count);
#endif
            if (!countSpan.consume(countSpan.size())) {
                throw gr::exception("consume failed");
            }
        }

        if (max_packets > 0 &&
            _entry_count - _exit_count >= static_cast<uint64_t>(max_packets)) {
            // cannot allow more packets into the latency management region
#ifdef TRACE
            fmt::println("{} latency management region full", this->name);
#endif
            outSpan.publish(0);
            return gr::work::Status::OK;
        }

        outSpan[0].data = std::vector<uint8_t>(packet_size);
        outSpan[0].tags = {};
        outSpan.publish(1);
        ++_entry_count;
        return gr::work::Status::OK;
    }
};

ENABLE_REFLECTION(PacketSource, count, out);

int main()
{
    using namespace gr::packet_modem;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<PacketSource>();
    auto& pdu_to_stream = fg.emplaceBlock<PduToTaggedStream<uint8_t>>();
    auto& packet_counter =
        fg.emplaceBlock<PacketCounter<uint8_t>>({ { "drop_tags", true } });
    auto& sink = fg.emplaceBlock<NullSink<uint8_t>>();

    const auto connection_error = "connection error";

    if (fg.connect<"out">(source).to<"in">(pdu_to_stream) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(pdu_to_stream).to<"in">(packet_counter) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"count">(packet_counter).to<"count">(source) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }
    if (fg.connect<"out">(packet_counter).to<"in">(sink) !=
        gr::ConnectionResult::SUCCESS) {
        throw gr::exception(connection_error);
    }

    gr::scheduler::Simple sched{ std::move(fg) };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println(stderr, "scheduler error: {}", ret.error());
        std::exit(1);
    }

    return 0;
}
