#ifndef _GR4_PACKET_MODEM_PACKET_INGRESS
#define _GR4_PACKET_MODEM_PACKET_INGRESS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <typename T = uint8_t>
class PacketIngress : public gr::Block<PacketIngress<T>>
{
public:
    using Description = Doc<R""(
@brief Packet Ingress. Accepts packets into a packet-based transmitter.

This block is the point at which packets enter into packet-based
transmitter. Packets are delimited at the input of this block by packet-length
tags, and presented back-to-back. For each valid packet, the block sends a
message to the header formatter indicating the length of the packet, so that the
header formatter can produce a header to be attached to the packet. Invalid
packets, which are those longer than the maximum packet length that fits in the
header, are consumed and discarded, and a warning is printed.

The block could be extended by the user to perform any other validation checks
or payload preparation that is necessary.

)"">;

private:
    const std::string d_packet_len_tag_key;
    size_t d_remaining;
    bool d_valid;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::MsgPortOut metadata;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    PacketIngress(const std::string& packet_len_tag_key = "packet_len")
        : d_packet_len_tag_key(packet_len_tag_key), d_remaining(0)
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "d_remaining = {}, d_valid = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     d_remaining,
                     d_valid);
#endif
        if (inSpan.size() == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        if (d_remaining == 0) {
            // Fetch a new packet_len tag
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
            d_valid = d_remaining <= std::numeric_limits<uint16_t>::max();
            if (d_valid) {
                // Use gr::message::Command::Invalid, since none of the OpenCMW
                // commands is appropriate for GNU Radio 3.10-style message passing.
                gr::sendMessage<gr::message::Command::Invalid>(
                    metadata, "", "", { { "packet_length", d_remaining } });
#ifdef TRACE
                fmt::println("{} publishTag({}, 0)", this->name, tag.map);
#endif
                out.publishTag(tag.map, 0);
            } else {
                fmt::println(
                    "{} packet too long (length {}); dropping", this->name, d_remaining);
            }
        } else if (d_valid && this->input_tags_present()) {
#ifdef TRACE
            fmt::println("{} publishTag({}, 0)", this->name, this->mergedInputTag().map);
#endif
            out.publishTag(this->mergedInputTag().map, 0);
        }

        auto to_consume = std::min(d_remaining, inSpan.size());
        if (d_valid) {
            to_consume = std::min(to_consume, outSpan.size());
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(to_consume), outSpan.begin());
            outSpan.publish(to_consume);
        } else {
            outSpan.publish(0);
        }
        std::ignore = inSpan.consume(to_consume);
        d_remaining -= to_consume;

        if (to_consume == 0) {
            return inSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketIngress, in, out);

#endif // _GR4_PACKET_PACKET_INGRESS
