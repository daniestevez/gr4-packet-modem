#ifndef _GR4_PACKET_MODEM_PACKET_INGRESS
#define _GR4_PACKET_MODEM_PACKET_INGRESS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
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

public:
    size_t _remaining;
    bool _valid;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    gr::MsgPortOut metadata;
    std::string packet_len_tag_key = "packet_len";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void start() { _remaining = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "_remaining = {}, _valid = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _remaining,
                     _valid);
#endif
        assert(inSpan.size() > 0);
        assert(inSpan.size() == outSpan.size());
        if (_remaining == 0) {
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
            if (!tag.map.contains(packet_len_tag_key)) {
                return not_found_error();
            }
            _remaining = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
            _valid = _remaining <= std::numeric_limits<uint16_t>::max();
            if (_valid) {
                // Use gr::message::Command::Invalid, since none of the OpenCMW
                // commands is appropriate for GNU Radio 3.10-style message passing.
                gr::sendMessage<gr::message::Command::Invalid>(
                    metadata, "", "", { { "packet_length", _remaining } });
#ifdef TRACE
                fmt::println("{} publishTag({}, 0)", this->name, tag.map);
#endif
                out.publishTag(tag.map, 0);
            } else {
                fmt::println(
                    "{} packet too long (length {}); dropping", this->name, _remaining);
            }
        } else if (_valid && this->input_tags_present()) {
#ifdef TRACE
            fmt::println("{} publishTag({}, 0)", this->name, this->mergedInputTag().map);
#endif
            out.publishTag(this->mergedInputTag().map, 0);
        }

        const auto to_consume = std::min(_remaining, inSpan.size());
        if (_valid) {
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(to_consume), outSpan.begin());
            outSpan.publish(to_consume);
        } else {
            outSpan.publish(0);
        }
        if (!inSpan.consume(to_consume)) {
            throw gr::exception("consume failed");
        }
        _remaining -= to_consume;
        assert(to_consume > 0);

        return gr::work::Status::OK;
    }
};


template <typename T>
class PacketIngress<Pdu<T>> : public gr::Block<PacketIngress<Pdu<T>>>
{
public:
    using Description = PacketIngress<T>::Description;

public:
    size_t _remaining;
    bool _valid;

public:
    gr::PortIn<Pdu<T>> in;
    gr::PortOut<Pdu<T>> out;
    gr::MsgPortOut metadata;
    std::string packet_len_tag_key = "packet_len";

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        assert(inSpan.size() > 0);
        assert(inSpan.size() == outSpan.size());
        size_t consumed = 0;
        size_t produced = 0;
        while (consumed < inSpan.size()) {
            const uint64_t packet_length = inSpan[consumed].data.size();
            if (packet_length <= std::numeric_limits<uint16_t>::max()) {
                outSpan[produced] = inSpan[consumed];
                // Use gr::message::Command::Invalid, since none of the OpenCMW
                // commands is appropriate for GNU Radio 3.10-style message passing.
                gr::sendMessage<gr::message::Command::Invalid>(
                    metadata, "", "", { { "packet_length", packet_length } });
                ++produced;
            } else {
                fmt::println("{} packet too long (length {}); dropping",
                             this->name,
                             packet_length);
            }
            ++consumed;
        }
        outSpan.publish(produced);
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketIngress,
                               in,
                               out,
                               packet_len_tag_key);

#endif // _GR4_PACKET_PACKET_INGRESS
