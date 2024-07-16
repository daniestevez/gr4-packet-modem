#ifndef _GR4_PACKET_MODEM_PACKET_TO_STREAM
#define _GR4_PACKET_MODEM_PACKET_TO_STREAM

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class PacketToStream : public gr::Block<PacketToStream<T>>
{
public:
    using Description = Doc<R""(
@brief Packet to Stream. Inserts zeros between packets if downstream requires
data but there is no input available.

This block is used to convert a packet-based transmission, where there can pass
a significant time between adjacent packets, into a stream-based transmission,
such as what is required to continuously feed a DAC at a constant sample rate.

The block works by copying packets from the input to the output. The input
packets are delimited by a packet-length tag in their first item which indicates
the length of the packet. Packets need to be present back-to-back in the
input. The block throws an exception if a packet-length tag is not present N
items after a packet-length tag, where N is the value of that tag. The key used
for the packet-length tag is indicated by the `packet_len_tag_key` constructor
parameter.

If the block `processBulk()` function is called because downstream needs samples
but there is not a new packet present at the input of this block, the block will
fill the output span with zeros and return. In this way, a discontinuous
packet-stream is converted into a continuous stream by inserting zeros between
packets as needed. If the block is in the process of copying an input packet to
the output when `processBulk()` is called, then it requires input to be present
to produce output, in order to finish the packet, instead of inserting zero
mid-packet.

The block has an optional `count` output that sends a message with the total
count of packets that have crossed this block every time that a new packet
crosses the block. This is intended to be used as part of a latency management
system (see PacketCounter).

)"">;

public:
    uint64_t _remaining;
    uint64_t _packet_count;

public:
    // The input port is declared as async to signal the runtime that we do not
    // need an input on this port to produce an output.
    gr::PortIn<T, gr::Async> in;
    gr::PortOut<T> out;
    gr::PortOut<gr::Message, gr::Async, gr::Optional> count;
    std::string packet_len_tag_key = "packet_len";

    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    void start()
    {
        _remaining = 0;
        _packet_count = 0;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan,
                                 gr::PublishableSpan auto& countSpan)
    {
        assert(countSpan.size() > 0);
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}, "
                     "countSpan.size = {}), "
                     "_remaining = {}, _packet_count = {}, input_tags_present = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     countSpan.size(),
                     _remaining,
                     _packet_count,
                     this->input_tags_present());
#endif
        if (_remaining == 0 && inSpan.size() == 0) {
            // We are not mid-packet and there is no input available. Fill the
            // output with zeros and return.
            std::ranges::fill(outSpan, T{ 0 });
            return gr::work::Status::OK;
        }

        if (_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            auto not_found_error = [this]() {
                this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                       "expected packet-length tag not found");
                this->requestStop();
                return gr::work::Status::ERROR;
            };
            if (!this->input_tags_present()) {
                return not_found_error();
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(packet_len_tag_key)) {
                return not_found_error();
            }
            _remaining = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
        }

        const auto to_publish = std::min({ inSpan.size(), outSpan.size(), _remaining });
        std::ranges::copy_n(
            inSpan.begin(), static_cast<ssize_t>(to_publish), outSpan.begin());
        _remaining -= to_publish;
        if (_remaining == 0) {
            ++_packet_count;
            gr::Message msg;
            msg.data = gr::property_map{ { "packet_count", _packet_count } };
            countSpan[0] = std::move(msg);
            countSpan.publish(1);
        } else {
            countSpan.publish(0);
        }

        // At this point we return instead of trying to fill the rest of the
        // output with zeros. There might be a packet available on the input but
        // not present in inSpan because the scheduler calls processBulk() with
        // one packet at a time, since tags are always aligned to the first item
        // of inSpan.

        if (!inSpan.consume(to_publish)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(to_publish);

#ifdef TRACE
        fmt::println("{}::processBulk published = {} ", this->name, to_publish);
#endif

        return gr::work::Status::OK;
    }
};

template <typename T>
class PacketToStream<Pdu<T>> : public gr::Block<PacketToStream<Pdu<T>>>
{
public:
    using Description = PacketToStream<T>::Description;

public:
    Pdu<T> _pdu;
    size_t _remaining;
    uint64_t _packet_count;

public:
    // The input port is declared as async to signal the runtime that we do not
    // need an input on this port to produce an output.
    gr::PortIn<Pdu<T>, gr::Async> in;
    gr::PortOut<T> out;
    gr::PortOut<gr::Message, gr::Async, gr::Optional> count;
    std::string packet_len_tag_key = "packet_len";

    void start()
    {
        _remaining = 0;
        _packet_count = 0;
    }

    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_DONT;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan,
                                 gr::PublishableSpan auto& countSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}, "
                     "countSpan.size() = {}), "
                     "_remaining = {}, _packet_count = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     countSpan.size(),
                     _remaining,
                     _packet_count);
#endif
        if (_remaining == 0 && inSpan.size() == 0) {
            // We are not mid-packet and there is no input available. Fill the
            // output with zeros and return.
            std::ranges::fill(outSpan, T{ 0 });
            return gr::work::Status::OK;
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        auto count_item = countSpan.begin();
        while (out_item < outSpan.end() && count_item < countSpan.end()) {
            if (_remaining == 0) {
                if (in_item >= inSpan.end()) {
                    // At this point we return instead of trying to fill the rest of the
                    // output with zeros. There might be a packet available on the input
                    // but not present in inSpan because the scheduler calls processBulk()
                    // with one packet at a time, since tags are always aligned to the
                    // first item of inSpan.
                    break;
                }
                // Fetch a PDU from the input
                _pdu = *in_item++;
                _remaining = _pdu.data.size();
            }

            const auto to_publish =
                std::min({ static_cast<size_t>(outSpan.end() - out_item), _remaining });
            std::ranges::copy_n(_pdu.data.cend() - static_cast<ssize_t>(_remaining),
                                static_cast<ssize_t>(to_publish),
                                out_item);
            _remaining -= to_publish;
            if (_remaining == 0) {
                ++_packet_count;
                gr::Message msg;
                msg.data = gr::property_map{ { "packet_count", _packet_count } };
                *count_item++ = std::move(msg);
            }
            out_item += static_cast<ssize_t>(to_publish);
        }

#ifdef TRACE
        fmt::println("{}::processBulk consumed = {} published = {} ",
                     this->name,
                     in_item - inSpan.begin(),
                     out_item - outSpan.begin());
        for (const auto& oi : outSpan | std::views::take(out_item - outSpan.begin())) {
            if (oi != T{ 0 }) {
                fmt::println("PacketToStream produced non-zero items!!!");
                break;
            }
        }
#endif

        if (!inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()))) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));
        countSpan.publish(static_cast<size_t>(count_item - countSpan.begin()));

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::PacketToStream, in, out, count, packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_PACKET_TO_STREAM
