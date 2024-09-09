#ifndef _GR4_PACKET_MODEM_SYNCWORD_DETECTION_FILTER
#define _GR4_PACKET_MODEM_SYNCWORD_DETECTION_FILTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>

namespace gr::packet_modem {

template <typename T = std::complex<float>>
class SyncwordDetectionFilter : public gr::Block<SyncwordDetectionFilter<T>>
{
public:
    using Description = Doc<R""(
@brief Syncword Detection Filter.

The Syncword Detection Filter is used to filter out false syncword detections
that may occur mid-packet if some of the scrambled packet payload correlates
strongly with the syncword. Without this block, such a false syncword detection
will likely prevent correct packet decoding by setting the Coarse Frequency
Correction to a wrong frequency mid-packet.

The Syncword Detection Filter detects the beginning of a packet by looking at
the syncword_ tags inserted by Syncword Detection. When a packet begins, the
block only lets the samples of the syncword and header, plus some small margin
go through. It waits to receive a header decode that either indicates the packet
length or an invalid header decode (in which case it immediately considers that
the packet has finished after the header). In this way, the Syncword Detection
Filter keeps track of wheter a particular sample is inside of a packet or
not. If syncword_ tags are received inside a packet, they are dropped.

)"">;

public:
    bool _in_packet = false;
    size_t _position = 0;
    size_t _block_until = 0;

public:
    gr::PortIn<gr::Message, gr::Async> parsed_header;
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    size_t samples_per_symbol = 4;
    size_t syncword_size = 64;
    size_t header_size = 128;
    size_t allowed_margin = 16; // symbols

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void start() { _in_packet = false; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& headerSpan,
                                 const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        using namespace std::string_literals;

#ifdef TRACE
        fmt::println("{}::processBulk(headerSpan.size() = {}, inSpan.size() = {}, "
                     "outSpan.size() = {}), _in_packet = {}, _position = {}, "
                     "_block_until = {}",
                     this->name,
                     headerSpan.size(),
                     inSpan.size(),
                     outSpan.size(),
                     _in_packet,
                     _position,
                     _block_until);
#endif
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            gr::property_map output_tags;
#ifdef TRACE
            fmt::println("{} tag.map = {}", this->name, tag.map);
#endif
            bool new_in_packet = false;
            for (auto const& [key, val] : tag.map) {
                if (key.starts_with("syncword_"s)) {
                    // only pass syncword tags to output if we're not inside a packet
                    if (!_in_packet) {
                        new_in_packet = true;
                        output_tags[key] = val;
                    }
                } else {
                    // non-syncword tag; pass it to output
                    output_tags[key] = val;
                }
            }
            if (new_in_packet) {
                _in_packet = true;
                _in_packet = true;
                _position = 0;
                _block_until = 0; // packet size yet unknown
            }
            if (!output_tags.empty()) {
#ifdef TRACE
                fmt::println("{} publishTag() output_tags = {}", this->name, output_tags);
#endif
                out.publishTag(output_tags, 0);
            }
        }

        if (!_in_packet) {
            const size_t n = std::min(inSpan.size(), outSpan.size());
            std::copy_n(inSpan.begin(), n, outSpan.begin());
            if (!headerSpan.consume(0)) {
                throw gr::exception("headerSpan.consume(0) failed");
            }
            if (!inSpan.consume(n)) {
                throw gr::exception(fmt::format("inSpan.consume({}) failed", n));
            }
            outSpan.publish(n);
            // TODO: not sure why this is needed here, since some output is being
            // published
            this->_mergedInputTag.map.clear();
#ifdef TRACE
            fmt::println("{} consumed = {}, header_consumed = 0", this->name, n, 0);
#endif
            return gr::work::Status::OK;
        }

        size_t header_consumed = 0;

        if (_block_until == 0 && headerSpan.size() > 0) {
            auto meta = headerSpan[0].data.value();
            header_consumed = 1;
            if (meta.contains("invalid_header")) {
                // header decode failed; we are no longer inside packet
                _block_until = 1;
            } else {
                const uint64_t packet_length =
                    pmtv::cast<uint64_t>(meta.at("packet_length"));
                if (packet_length == 0) {
                    throw gr::exception("received packet_length = 0");
                }
                // packet_length is in bytes, and we use QPSK modulation for the
                // packet. We also need to add the CRC-32
                constexpr size_t crc_size_bytes = 4;
                const size_t payload_symbols = (packet_length + crc_size_bytes) * 4;
                _block_until = samples_per_symbol * (header_size + syncword_size -
                                                     allowed_margin + payload_symbols);
            }
        }

        size_t consumed = 0;

        const size_t allowed =
            samples_per_symbol * (syncword_size + header_size + allowed_margin);
        if (_position < allowed) {
            const size_t n =
                std::min({ inSpan.size(), outSpan.size(), allowed - _position });
            std::copy_n(inSpan.begin(), n, outSpan.begin());
            _position += n;
            consumed = n;
        }

        if (_position >= allowed && _block_until != 0) {
            // packet size already known
            const size_t n = std::min(inSpan.size(), outSpan.size()) - consumed;
            std::copy_n(inSpan.begin(), n, outSpan.begin());
            _position += n;
            consumed += n;
            if (_position >= _block_until) {
                _in_packet = false;
            }
        }

        if (!inSpan.consume(consumed)) {
            throw gr::exception(fmt::format("inSpan.consume({}) failed", consumed));
        }
        if (!headerSpan.consume(header_consumed)) {
            throw gr::exception(fmt::format("headerSpan.consume({})", header_consumed));
        }
        outSpan.publish(consumed);
        // TODO: not sure why this is needed here, since some output is being
        // published
        this->_mergedInputTag.map.clear();
#ifdef TRACE
        fmt::println("{} consumed = {}, header_consumed = {}",
                     this->name,
                     consumed,
                     header_consumed);
#endif
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::SyncwordDetectionFilter,
                               parsed_header,
                               in,
                               out,
                               samples_per_symbol,
                               syncword_size,
                               header_size);

#endif // _GR4_PACKET_MODEM_SYNCWORD_DETECTION_FILTER
