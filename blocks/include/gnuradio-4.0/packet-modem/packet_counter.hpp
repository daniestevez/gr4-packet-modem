#ifndef _GR4_PACKET_MODEM_PACKET_COUNTER
#define _GR4_PACKET_MODEM_PACKET_COUNTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <typename T = uint8_t>
class PacketCounter : public gr::Block<PacketCounter<T>>
{
public:
    using Description = Doc<R""(
@brief Packet Counter.

This block is part of a latency management system. It counts packets going
through it. Each time that a packet crosses the block, it produces an output
message indicating the total number of packets that have crossed the block. The
block can be placed at the output of latency management region to count the
number of packets that have exited the region.

)"">;

public:
    uint64_t _count;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::PortOut<gr::Message, gr::Async> count;
    std::string packet_len_tag_key = "packet_len";
    bool drop_tags = false;

    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    void start() { _count = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan,
                                 gr::PublishableSpan auto& countSpan)
    {
        assert(inSpan.size() == outSpan.size());
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}, "
                     "countSpan.size() = {}), _count = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     countSpan.size(),
                     _count);
#endif
        if (this->input_tags_present()) {
            const auto tag = this->mergedInputTag();
            if (tag.map.contains(packet_len_tag_key)) {
                ++_count;
                gr::Message msg;
                msg.data = gr::property_map{ { "packet_count", _count } };
                countSpan[0] = std::move(msg);
                countSpan.publish(1);
#ifdef TRACE
                fmt::println("{} incremented _count", this->name);
#endif
            } else {
                countSpan.publish(0);
            }
            if (!drop_tags) {
              out.publishTag(tag.map, 0);
            }
        } else {
            countSpan.publish(0);
        }
        std::copy_n(inSpan.begin(), inSpan.size(), outSpan.begin());
        // this should be done automatically, but just in case
        if (!inSpan.consume(inSpan.size())) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(outSpan.size());
        return gr::work::Status::OK;
    }
};

template <typename T>
class PacketCounter<Pdu<T>> : public gr::Block<PacketCounter<Pdu<T>>>
{
public:
    using Description = PacketCounter<T>::Description;

public:
    uint64_t _count;

public:
    gr::PortIn<Pdu<T>> in;
    gr::PortOut<Pdu<T>> out;
    gr::PortOut<gr::Message> count;
    std::string packet_len_tag_key = "packet_len";
    bool drop_tags = false; // ignored in Pdu mode

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan,
                                 gr::PublishableSpan auto& countSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}, "
                     "countSpan.size() = {}), _count = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     countSpan.size(),
                     _count);
#endif
        assert(inSpan.size() == outSpan.size());
        assert(inSpan.size() == countSpan.size());
        for (size_t j = 0; j < inSpan.size(); ++j) {
            outSpan[j] = inSpan[j];
            ++_count;
            gr::Message msg;
            msg.data = gr::property_map{ { "packet_count", _count } };
            countSpan[j] = std::move(msg);
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::PacketCounter, in, out, count, packet_len_tag_key, drop_tags);

#endif // _GR4_PACKET_PACKET_COUNTER
