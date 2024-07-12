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

    void start() { _count = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan,
                                 gr::PublishableSpan auto& countSpan)
    {
        throw gr::exception("this block is not implemented");
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
    gr::packet_modem::PacketCounter, in, out, count, packet_len_tag_key);

#endif // _GR4_PACKET_PACKET_COUNTER
