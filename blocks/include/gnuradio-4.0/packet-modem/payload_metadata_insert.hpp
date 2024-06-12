#ifndef _GR4_PACKET_MODEM_PAYLOAD_METADATA_INSERT
#define _GR4_PACKET_MODEM_PAYLOAD_METADATA_INSERT

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>

namespace gr::packet_modem {

template <typename T = std::complex<float>>
class PayloadMetadataInsert : public gr::Block<PayloadMetadataInsert<T>>
{
public:
    using Description = Doc<R""(
@brief Payload Metadata Insert.

This block receives a stream of symbols marked with `"syncword_amplitude"` tags
at the locations where a syncword begins. The block inserts `"constellation":
"BPSK"` and `"constellation": "QPSK"` tags at the beginning of the syncword and
header so that a downstream Costas loop can use the correct phase error
detector. Before passing the payload symbols to the output, the block waits to
receive a message containing the metadata from the decoded header in the
`parsed_header` input port. This message indicates the payload length, and is
included as a tag at the beginning of the payload. The payload symbols are
passed to the output, but the remaining symbols after the payload (according to
the `"packet_length"` in the header metadata, which indicates the packet length
in bytes) are dropped.

The sizes of the syncword and header are indicated by the `syncword_size` and
`header_size` parameters.

)"">;

public:
    bool _in_packet = false;
    uint64_t _position = 0;
    size_t _packet_symbols = 0;

private:
    static constexpr char syncword_amplitude_key[] = "syncword_amplitude";
    static constexpr char constellation_key[] = "constellation";
    static constexpr char bpsk_key[] = "BPSK";
    static constexpr char qpsk_key[] = "QPSK";
    static constexpr char packet_length_key[] = "packet_length";

public:
    gr::PortIn<gr::Message, gr::Async> parsed_header;
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    size_t syncword_size = 64;
    size_t header_size = 128;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& headerSpan,
                                 const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(headerSpan.size() = {}, inSpan.size() = {}, "
                     "outSpan.size() = {}), _in_packet = {}, _position = {}",
                     this->name,
                     headerSpan.size(),
                     inSpan.size(),
                     outSpan.size(),
                     _in_packet,
                     _position);
#endif
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(syncword_amplitude_key)) {
#ifdef TRACE
                fmt::println("{} got {} tag", this->name, syncword_amplitude_key);
#endif
                _in_packet = true;
                _position = 0;
                // the syncword is BPSK modulated
                tag.map[constellation_key] = bpsk_key;
                out.publishTag(tag.map, 0);
            }
        }
        if (!_in_packet) {
            // discard all the input
            if (!headerSpan.consume(0)) {
                throw gr::exception("consume failed");
            }
            if (!inSpan.consume(inSpan.size())) {
                throw gr::exception("consume failed");
            }
            outSpan.publish(0);
            return gr::work::Status::OK;
        }

        auto out_item = outSpan.begin();
        auto in_item = inSpan.begin();
        auto header_item = headerSpan.begin();
        while (out_item < outSpan.end() && in_item < inSpan.end()) {
            if (_position < syncword_size) {
                // copy rest of syncword to output
                const auto n = std::min({ static_cast<size_t>(inSpan.end() - in_item),
                                          static_cast<size_t>(outSpan.end() - out_item),
                                          syncword_size - _position });
                std::copy_n(in_item, n, out_item);
                in_item += static_cast<ssize_t>(n);
                out_item += static_cast<ssize_t>(n);
                _position += n;
            }

            if (_position == syncword_size) {
                // the header is QPSK modulated
                out.publishTag({ { constellation_key, qpsk_key } },
                               out_item - outSpan.begin());
            }

            if (syncword_size <= _position && _position < syncword_size + header_size) {
                // copy rest of header to output
                const auto n = std::min({ static_cast<size_t>(inSpan.end() - in_item),
                                          static_cast<size_t>(outSpan.end() - out_item),
                                          syncword_size + header_size - _position });
                std::copy_n(in_item, n, out_item);
                in_item += static_cast<ssize_t>(n);
                out_item += static_cast<ssize_t>(n);
                _position += n;
            }

            if (_position == syncword_size + header_size && out_item < outSpan.end() &&
                in_item < inSpan.end()) {
                if (header_item < headerSpan.end()) {
                    // we have decoded the header corresponding to this payload
                    const auto meta = header_item->data.value();
                    const uint64_t packet_length =
                        pmtv::cast<uint64_t>(meta.at(packet_length_key));
                    if (packet_length == 0) {
                        throw gr::exception("received packet_length = 0");
                    }
                    out.publishTag(meta, out_item - outSpan.begin());
                    // packet_length is in bytes, and we use QPSK modulation for the
                    // packet. We also need to add the CRC-32
                    constexpr size_t crc_size_bytes = 4;
                    _packet_symbols = (packet_length + crc_size_bytes) * 4;

                    const auto n =
                        std::min({ static_cast<size_t>(inSpan.end() - in_item),
                                   static_cast<size_t>(outSpan.end() - out_item),
                                   _packet_symbols });
                    std::copy_n(in_item, n, out_item);
                    in_item += static_cast<ssize_t>(n);
                    out_item += static_cast<ssize_t>(n);
                    _position += n;
                } else {
                    // the header for this payload has not been decoded
                    // yet. return to wait for it to be decoded
                    break;
                }
            }

            if (syncword_size + header_size < _position &&
                _position < syncword_size + header_size + _packet_symbols) {
                // copy rest of payload
                const auto n = std::min({ static_cast<size_t>(inSpan.end() - in_item),
                                          static_cast<size_t>(outSpan.end() - out_item),
                                          _packet_symbols });
                std::copy_n(in_item, n, out_item);
                in_item += static_cast<ssize_t>(n);
                out_item += static_cast<ssize_t>(n);
                _position += n;
            }

            if (_position >= syncword_size + header_size + _packet_symbols) {
                // end of packet
                _in_packet = false;
                break;
            }
        }

        if (!headerSpan.consume(static_cast<size_t>(header_item - headerSpan.begin()))) {
            throw gr::exception("consume failed");
        }
        if (!inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()))) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));
#ifdef TRACE
        fmt::println("{} consumed = {}, published = {}",
                     this->name,
                     in_item - inSpan.begin(),
                     out_item - outSpan.begin());
#endif

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PayloadMetadataInsert,
                               parsed_header,
                               in,
                               out,
                               syncword_size,
                               header_size);

#endif // _GR4_PACKET_MODEM_PAYLOAD_METADATA_INSERT
