#ifndef _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU
#define _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class TaggedStreamToPdu : public gr::Block<TaggedStreamToPdu<T>>
{
public:
    using Description = Doc<R""(
@brief Tagged Stream to PDU. Converts a stream of packets delimited by packet length tags
into a stream of PDUs.

The input of this block is a stream of items of type `T` forming packets
delimited by a packet-length tag that is present in the first item of the packet
and whose value indicates the length of the packet. It converts the packets in
the input into a stream of `Pdu` objects, where each packet is stored as the
`data` vector in the `Pdu`, and the tags previously attached to items of the
packet are stored as the `tags` vector in the `Pdu`.

)"">;

private:
    const std::string d_packet_len_tag_key;
    uint64_t d_remaining = 0;
    uint64_t d_index;
    Pdu<T> d_pdu;

public:
    gr::PortIn<T> in;
    gr::PortOut<Pdu<T> /*, gr::RequiredSamples<1U, 1U, false>*/> out;
    // This causes compile errors:
    // gr::PortOut<Pdu<T>, gr::RequiredSamples<1U, 1U, true>> out;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    TaggedStreamToPdu(const std::string& packet_len_tag_key = "packet_len")
        : d_packet_len_tag_key(packet_len_tag_key)
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = "
                     "{}), d_remaining = {}, d_pdu.data.size() = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     d_remaining,
                     d_pdu.data.size());
#endif
        if (inSpan.size() == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        if (outSpan.size() == 0) {
            // we always require space in the output, just in case we need to
            // publish an output PDU
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        if (d_remaining == 0) {
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
            if (!tag.map.contains(d_packet_len_tag_key)) {
                return not_found_error();
            }
            d_remaining = pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
            if (d_remaining == 0) {
                this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                       "received packet-length equal to zero");
                this->requestStop();
                return gr::work::Status::ERROR;
            }
            d_pdu.data.clear();
            d_pdu.data.reserve(d_remaining);
            d_pdu.tags.clear();
            d_index = 0;
        }

        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            tag.index = static_cast<ssize_t>(d_index);
            // remove packet_len tag
            if (d_index == 0) {
                tag.map.erase(d_packet_len_tag_key);
            }
            // the map can be empty if it only contained the packet_len tag
            if (!tag.map.empty()) {
                d_pdu.tags.push_back(tag);
            }
        }

        const auto to_consume = std::min(d_remaining, inSpan.size());
        std::ranges::copy(inSpan | std::views::take(to_consume),
                          std::back_inserter(d_pdu.data));
        std::ignore = inSpan.consume(to_consume);
        d_remaining -= to_consume;
        d_index += to_consume;

        if (d_remaining == 0) {
            outSpan[0] = std::move(d_pdu);
            d_pdu = {};
            outSpan.publish(1);
#ifdef TRACE
            fmt::println("{} consume = {}, publish = 1", this->name, to_consume);
#endif
        } else {
            outSpan.publish(0);
#ifdef TRACE
            fmt::println("{} consume = {}, publish = 0, d_remaining = {}, "
                         "d_pdu.data.size() = {}",
                         this->name,
                         to_consume,
                         d_remaining,
                         d_pdu.data.size());
#endif
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::TaggedStreamToPdu, in, out);

#endif // _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU
