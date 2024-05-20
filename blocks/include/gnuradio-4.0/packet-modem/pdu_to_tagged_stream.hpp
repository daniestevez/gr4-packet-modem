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
    const std::string d_packet_len_tag_key;
    ssize_t d_index = 0;

public:
    gr::PortIn<Pdu<T>> in;
    gr::PortOut<T, gr::Async> out;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    PduToTaggedStream(const std::string& packet_len_tag_key = "packet_len")
        : d_packet_len_tag_key(packet_len_tag_key)
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("PduToTaggedStream::processBulk(inSpan.size() = {}, outSpan.size() "
                     "= {}), d_index = {}",
                     inSpan.size(),
                     outSpan.size(),
                     d_index);
#endif
        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        while (in_item != inSpan.end() && out_item != outSpan.end()) {
            if (d_index == 0) {
                const uint64_t packet_len = in_item->data.size();
                if (packet_len == 0) {
                    throw std::runtime_error(
                        "[PduToTaggedStream] received PDU of length zero");
                }
                out.publishTag({ { d_packet_len_tag_key, packet_len } },
                               out_item - outSpan.begin());
            }
            const auto n =
                std::min(std::ssize(in_item->data) - d_index, outSpan.end() - out_item);
            std::ranges::copy(in_item->data | std::views::drop(d_index) |
                                  std::views::take(n),
                              out_item);
            for (const auto& tag : in_item->tags) {
                if (tag.index >= d_index && tag.index - d_index < n) {
                    out.publishTag(tag.map,
                                   tag.index - d_index + (out_item - outSpan.begin()));
                }
            }
            out_item += n;
            d_index += n;
            if (d_index == std::ssize(in_item->data)) {
                ++in_item;
                d_index = 0;
            }
        }

        std::ignore = inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()));
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));

        return out_item == outSpan.begin() ? gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS
                                           : gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PduToTaggedStream, in, out);

#endif // _GR4_PACKET_MODEM_PDU_TO_TAGGED_STREAM