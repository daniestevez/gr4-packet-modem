#ifndef _GR4_PACKET_MODEM_PACKET_LIMITER
#define _GR4_PACKET_MODEM_PACKET_LIMITER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <typename T = uint8_t>
class PacketLimiter : public gr::Block<PacketLimiter<T>>
{
public:
    using Description = Doc<R""(
@brief Packet Limiter.

This block is part of a latency management system. It receives count messages
from a PacketCount block and uses them to limit the number of packets that can
enter at any given time into the latency management region.

The maximum number of packets in the region is defined by `max_packets`. The
special case `max_packets = 0` indicates that the number of packets in the
region is unlimited.

)"">;

public:
    uint64_t _exit_count;
    uint64_t _entry_count;

public:
    gr::PortIn<T, gr::Async> in;
    gr::PortIn<gr::Message, gr::Async> count;
    gr::PortOut<T, gr::Async> out;
    size_t max_packets = 0UZ;
    std::string packet_len_tag_key = "packet_len";

    void start()
    {
        _entry_count = 0;
        _exit_count = 0;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 const gr::ConsumableSpan auto& countSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        throw gr::exception("this block is not implemented");
    }
};


template <typename T>
class PacketLimiter<Pdu<T>> : public gr::Block<PacketLimiter<Pdu<T>>>
{
public:
    using Description = PacketLimiter<T>::Description;

public:
    uint64_t _exit_count;
    uint64_t _entry_count;

public:
    gr::PortIn<Pdu<T>, gr::Async> in;
    gr::PortIn<gr::Message, gr::Async> count;
    gr::PortOut<Pdu<T>, gr::Async> out;
    size_t max_packets = 0UZ;
    std::string packet_len_tag_key = "packet_len";

    void start()
    {
        _entry_count = 0;
        _exit_count = 0;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 const gr::ConsumableSpan auto& countSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, countSpan.size() = {}, "
                     "outSpan.size() = {}), _entry_count = {}, _exit_count = {}",
                     this->name,
                     inSpan.size(),
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
        size_t consumed = 0;
        while (consumed < inSpan.size() && consumed < outSpan.size()) {
            if (max_packets > 0 &&
                _entry_count - _exit_count >= static_cast<uint64_t>(max_packets)) {
                // cannot allow more packets into the region
                break;
            }
            outSpan[consumed] = inSpan[consumed];
            ++consumed;
            ++_entry_count;
        }
        if (!inSpan.consume(consumed)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(consumed);
#ifdef TRACE
        fmt::println("{} consumed & produced = {}", this->name, consumed);
#endif
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::PacketLimiter, in, count, out, max_packets, packet_len_tag_key);

#endif // _GR4_PACKET_PACKET_LIMITER
