#ifndef _GR4_PACKET_MODEM_HEADER_FORMATTER
#define _GR4_PACKET_MODEM_HEADER_FORMATTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <pmtv/pmt.hpp>

namespace gr::packet_modem {

static constexpr size_t HEADER_FORMATTER_HEADER_LEN = 4U;

class HeaderFormatter
    : public gr::Block<HeaderFormatter, gr::ResamplingRatio<HEADER_FORMATTER_HEADER_LEN>>
{
public:
    using Description = Doc<R""(
@brief Header Formatter. Converts messages with metadata into formatted packet
headers.

This block outputs a packet header into the output stream `out` for each message
received in the `metadata` port. The packet header is formed by the concatenation
of the following fields:

- Packet length. Big-endian 16 bits. Indicates the length of the payload in bytes.

- Packet type. 8 bits. Currently only the value `0x00` is used to denote uncoded
  QPSK-modulated payload. The values supported by this field can be extended in
  the future.

- Spare. 8 bits. Field to support future extensions and user customization. The
  spare field is filled with the value `0x55`.

The input messages need to contain the following properties:

- "packet_length". Indicates the length of the payload in bytes. Its value is
  used to fill the Packet length field in the header.

This list of properties can be extended in future versions or by user
customization. If some of the properties in this list are missing from a
message, the block returns an error. The input messages can contain
additional properties not included in this list, but they are ignored.

)"">;

private:
    const gr::property_map d_packet_len_tag;

    static constexpr size_t HEADER_LEN = HEADER_FORMATTER_HEADER_LEN;

public:
    gr::PortInNamed<gr::Message, "metadata"> metadata;
    gr::PortOut<uint8_t> out;

    HeaderFormatter(const std::string& packet_len_tag_key = "packet_len")
        : d_packet_len_tag({ { packet_len_tag_key, HEADER_LEN } })
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        const size_t num_headers = std::min(inSpan.size(), outSpan.size() / HEADER_LEN);

        if (num_headers == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return inSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        auto header = outSpan.begin();
        for (const auto& meta : inSpan | std::views::take(num_headers)) {
            out.publishTag(d_packet_len_tag, header - outSpan.begin());

            uint64_t packet_length = 0;
            try {
                packet_length =
                    pmtv::cast<uint64_t>(meta.data.value().at("packet_length"));
            } catch (...) {
                this->emitErrorMessage(fmt::format("{}:processBulk", this->name),
                                       "packet_length not present in metadata or cannot "
                                       "be cast to uint64_t");
                this->requestStop();
                return gr::work::Status::ERROR;
            }
            if (packet_length > std::numeric_limits<uint16_t>::max()) {
                this->emitErrorMessage(
                    fmt::format("{}::processBulk", this->name),
                    fmt::format("packet_length {} is too large", packet_length));
                this->requestStop();
                return gr::work::Status::ERROR;
            }

            header[0] = (packet_length >> 8) & 0xff;
            header[1] = packet_length & 0xff;
            header[2] = 0x00;
            header[3] = 0x55;
            header += HEADER_LEN;
        }

        std::ignore = inSpan.consume(num_headers);
        outSpan.publish(num_headers * HEADER_LEN);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::HeaderFormatter, metadata, out);

#endif // _GR4_PACKET_MODEM_HEADER_FORMATTER
