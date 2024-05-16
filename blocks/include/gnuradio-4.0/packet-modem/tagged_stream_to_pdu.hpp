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


)"">;

private:
    const std::string d_packet_len_tag_key;
    uint64_t d_remaining = 0;
    uint64_t d_index;
    Pdu<T> d_pdu;

public:
    gr::PortIn<T> in;
    gr::PortOut<Pdu<T>> out;

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
        fmt::print(
            "TaggedStreamToPdu::processBulk(inSpan.size() = {}, outSpan.size = {})\n",
            inSpan.size(),
            outSpan.size());
#endif
        if (outSpan.size() == 0) {
            // we always require space in the output, just in case we need to
            // publish an output PDU
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        if (d_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            static constexpr auto not_found =
                "[TaggedStreamToPdu] expected packet-length tag not found\n";
            if (!this->input_tags_present()) {
                throw std::runtime_error(not_found);
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(d_packet_len_tag_key)) {
                throw std::runtime_error(not_found);
            }
            d_remaining = pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
            if (d_remaining == 0) {
                throw std::runtime_error(
                    "[TaggedStreamToPdu] received packet_len equal to zero");
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
        } else {
            outSpan.publish(0);
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::TaggedStreamToPdu, in, out);

#endif // _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU
