#ifndef _GR4_PACKET_MODEM_PDU_TO_TAGGED_STREAM
#define _GR4_PACKET_MODEM_PDU_TO_TAGGED_STREAM

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class PduToTaggedStream : public gr::Block<PduToTaggedStream<T>>
{
public:
    using Description = Doc<R""(
@brief PDU to Tagged Stream. Converts a stream of PDUs into a stream of packets
delimited by packet length tags.

The input of this block is a stream of `Pdu` objects. It converts this into a
stream of items of type `T` by using packet-length tags to delimit the packets.
The tags in the `tag` vector of the `Pdu`s are attached to the corresponding
items in the output stream.

Tags in the input stream are discarded.

)"">;

private:
    ssize_t _index = 0;

public:
    gr::PortIn<Pdu<T>> in;
    gr::PortOut<T, gr::Async> out;
    std::string packet_len_tag_key = "packet_len";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void start() { _index = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println(
            "{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), _index = {}",
            this->name,
            inSpan.size(),
            outSpan.size(),
            _index);
#endif
        if (inSpan.size() == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        while (in_item != inSpan.end() && out_item != outSpan.end()) {
            if (_index == 0) {
                const uint64_t packet_len = in_item->data.size();
                if (packet_len == 0) {
                    this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                           "received PDU of length zero");
                    this->requestStop();
                    return gr::work::Status::ERROR;
                }
                out.publishTag({ { packet_len_tag_key, packet_len } },
                               out_item - outSpan.begin());
            }
            const auto n =
                std::min(std::ssize(in_item->data) - _index, outSpan.end() - out_item);
            std::ranges::copy(
                in_item->data | std::views::drop(_index) | std::views::take(n), out_item);
            for (const auto& tag : in_item->tags) {
                if (tag.index >= _index && tag.index - _index < n) {
                    out.publishTag(tag.map,
                                   tag.index - _index + (out_item - outSpan.begin()));
                }
            }
            out_item += n;
            _index += n;
            if (_index == std::ssize(in_item->data)) {
                ++in_item;
                _index = 0;
            }
        }

        std::ignore = inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()));
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));
#ifdef TRACE
        fmt::println("{} consumed = {}, published = {}",
                     this->name,
                     in_item - inSpan.begin(),
                     out_item - outSpan.begin());
#endif

        return out_item == outSpan.begin() ? gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS
                                           : gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PduToTaggedStream,
                               in,
                               out,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_PDU_TO_TAGGED_STREAM
