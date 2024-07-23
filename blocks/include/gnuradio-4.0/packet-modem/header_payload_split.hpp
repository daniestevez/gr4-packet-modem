#ifndef _GR4_PACKET_MODEM_HEADER_PAYLOAD_SPLIT
#define _GR4_PACKET_MODEM_HEADER_PAYLOAD_SPLIT

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T = float>
class HeaderPayloadSplit : public gr::Block<HeaderPayloadSplit<T>>
{
public:
    using Description = Doc<R""(
@brief Header Payload Split.

This block splits the header and the payload bits of packets into two different
outputs. The header has fixed size indicated by the `header_size` parameter. The
length of the payload is indicated by the "payload_bits" tag present at the
beginning of the payload (the tag key can be changed with the
`payload_length_key` template parameter). Packets must be present back-to-back
in the input.

)"">;

public:
    bool _in_payload = false;
    uint64_t _position = 0;
    uint64_t _payload_items = 0;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> header;
    gr::PortOut<T> payload;
    size_t header_size = 256;
    std::string packet_len_tag_key = "packet_len";
    std::string payload_length_key = "payload_bits";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void start()
    {
        _in_payload = false;
        _position = 0;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& headerSpan,
                                 gr::PublishableSpan auto& payloadSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, headerSpan.size() = {}, "
                     "payloadSpan.size() = {}), _in_payload = {}, _position = {}, "
                     "_payload_items = {}",
                     this->name,
                     inSpan.size(),
                     headerSpan.size(),
                     payloadSpan.size(),
                     _in_payload,
                     _position,
                     _payload_items);
#endif
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
#ifdef TRACE            
            fmt::println("{} got tag {}", this->name, tag.map);
#endif
            if (tag.map.contains(payload_length_key)) {
                if (_in_payload || _position != header_size) {
                    throw gr::exception(
                        fmt::format("received unexpected {} tag", payload_length_key));
                }
                _in_payload = true;
                _position = 0;
                _payload_items = pmtv::cast<uint64_t>(tag.map.at(payload_length_key));
                tag.map[packet_len_tag_key] = pmtv::pmt(_payload_items);
            }
            if (_in_payload) {
                payload.publishTag(tag.map);
            } else {
                header.publishTag(tag.map);
            }
        }

        if (!_in_payload && _position == header_size) {
            // header decode has failed, so a payload_length_key has not been inserted
            // upstream and no payload symbols have been output. return to beginning
            // of header
            _position = 0;
        }

        if (!_in_payload) {
            const auto n =
                std::min({ inSpan.size(), headerSpan.size(), header_size - _position });
            std::copy_n(inSpan.begin(), n, headerSpan.begin());
            if (n > 0 && _position == 0) {
                const property_map tag = { { packet_len_tag_key,
                                             static_cast<uint64_t>(header_size) } };
                header.publishTag(tag);
            }
            _position += n;
            if (!inSpan.consume(n)) {
                throw gr::exception("consume failed");
            }
            headerSpan.publish(n);
            payloadSpan.publish(0);
#ifdef TRACE
            fmt::println("{} published header = {}", this->name, n);
#endif
        } else {
            const auto n = std::min(
                { inSpan.size(), payloadSpan.size(), _payload_items - _position });
            std::copy_n(inSpan.begin(), n, payloadSpan.begin());
            _position += n;
            if (!inSpan.consume(n)) {
                throw gr::exception("consume failed");
            }
            headerSpan.publish(0);
            payloadSpan.publish(n);
#ifdef TRACE
            fmt::println("{} published payload = {}", this->name, n);
#endif
            if (_position >= _payload_items) {
                _in_payload = false;
                _position = 0;
            }
        }

        // TODO: not sure why this is needed here, since some output is being published
        this->_mergedInputTag.map.clear();

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::HeaderPayloadSplit,
                               in,
                               header,
                               payload,
                               header_size,
                               packet_len_tag_key,
                               payload_length_key);

#endif // _GR4_PACKET_MODEM_HEADER_PAYLOAD_SPLIT
