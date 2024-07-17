#ifndef _GR4_PACKET_MODEM_HEADER_FORMATTER
#define _GR4_PACKET_MODEM_HEADER_FORMATTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/packet_type.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <pmtv/pmt.hpp>

namespace gr::packet_modem {

static constexpr size_t HEADER_FORMATTER_HEADER_LEN = 4U;

template <typename T = uint8_t>
class HeaderFormatter : public gr::Block<HeaderFormatter<T>,
                                         gr::Resampling<1U, HEADER_FORMATTER_HEADER_LEN>>
{
public:
    using Description = Doc<R""(
@brief Header Formatter. Converts messages with metadata into formatted packet
headers.

This block outputs a packet header into the output stream `out` for each message
received in the `metadata` port. The packet header is formed by the concatenation
of the following fields:

- Packet length. Big-endian 16 bits. Indicates the length of the payload in bytes.

- Packet type. 8 bits. Currently, the value `0x00` is used to denote uncoded
  QPSK-modulated payload containing user data, and the value `0x01` is used to
  denote uncoded QPSK-modulated payload containing idle data (an idle
  packet). The values supported by this field can be extended in the future to
  indicate other MODCODs or other types of data, such as control information.

- Spare. 8 bits. Field to support future extensions and user customization. The
  spare field is filled with the value `0x55`.

The input messages need to contain the following properties:

- "packet_length". Indicates the length of the payload in bytes. Its value is
  used to fill the Packet length field in the header.

- "packet_type". Indicates the type of the packet, using the `PacketType` enum
  (converted to a string).

This list of properties can be extended in future versions or by user
customization. If some of the properties in this list are missing from a
message, the block returns an error. The input messages can contain
additional properties not included in this list, but they are ignored.

)"">;

private:
    static constexpr size_t HEADER_LEN = HEADER_FORMATTER_HEADER_LEN;

public:
    gr::PortIn<gr::Message> metadata;
    gr::PortOut<uint8_t> out;
    std::string packet_len_tag_key = "packet_len";

public:
    gr::property_map _packet_len_tag = { { packet_len_tag_key, HEADER_LEN } };

public:
    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        _packet_len_tag = { { packet_len_tag_key, HEADER_LEN } };
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
        assert(inSpan.size() == outSpan.size() / HEADER_LEN);
        assert(inSpan.size() > 0);
        auto header = outSpan.begin();
        for (const auto& meta : inSpan) {
            out.publishTag(_packet_len_tag, header - outSpan.begin());

            uint64_t packet_length = 0;
            const auto& meta_map = meta.data.value();
            try {
                packet_length = pmtv::cast<uint64_t>(meta_map.at("packet_length"));
            } catch (...) {
                this->emitErrorMessage(fmt::format("{}:processBulk", this->name),
                                       "packet_length not present in metadata or cannot "
                                       "be cast to uint64_t");
                this->requestStop();
                return gr::work::Status::ERROR;
            }
            const auto packet_type =
                magic_enum::enum_cast<PacketType>(
                    pmtv::cast<std::string>(meta_map.at("packet_type")),
                    magic_enum::case_insensitive)
                    .value();
            if (packet_length > std::numeric_limits<uint16_t>::max()) {
                this->emitErrorMessage(
                    fmt::format("{}::processBulk", this->name),
                    fmt::format("packet_length {} is too large", packet_length));
                this->requestStop();
                return gr::work::Status::ERROR;
            }

            header[0] = (packet_length >> 8) & 0xff;
            header[1] = packet_length & 0xff;
            header[2] = packet_type == PacketType::IDLE ? 0x01 : 0x00;
            header[3] = 0x55;
            header += HEADER_LEN;
        }

        return gr::work::Status::OK;
    }
};

template <>
class HeaderFormatter<Pdu<uint8_t>> : public gr::Block<HeaderFormatter<Pdu<uint8_t>>>
{
public:
    using Description = HeaderFormatter<uint8_t>::Description;

private:
    static constexpr size_t HEADER_LEN = HEADER_FORMATTER_HEADER_LEN;

public:
    gr::PortIn<gr::Message> metadata;
    gr::PortOut<Pdu<uint8_t>> out;
    std::string packet_len_tag_key = "packet_len";

public:
    gr::property_map _packet_len_tag = { { packet_len_tag_key, HEADER_LEN } };

public:
    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        _packet_len_tag = { { packet_len_tag_key, HEADER_LEN } };
    }

    [[nodiscard]] Pdu<uint8_t> processOne(const gr::Message& meta)
    {
        const auto& meta_map = meta.data.value();
        const auto packet_length = pmtv::cast<uint64_t>(meta_map.at("packet_length"));
        if (packet_length > std::numeric_limits<uint16_t>::max()) {
            throw gr::exception(
                fmt::format("packet_length {} is too large", packet_length));
        }
        const auto packet_type = magic_enum::enum_cast<PacketType>(
                                     pmtv::cast<std::string>(meta_map.at("packet_type")),
                                     magic_enum::case_insensitive)
                                     .value();
#ifdef TRACE
        fmt::println("{} packet_length = {}, packet_type = {}",
                     this->name,
                     packet_length,
                     magic_enum::enum_name(packet_type));
#endif

        Pdu<uint8_t> header;
        header.data.push_back((packet_length >> 8) & 0xff);
        header.data.push_back(packet_length & 0xff);
        header.data.push_back(packet_type == PacketType::IDLE ? 0x01 : 0x00);
        header.data.push_back(0x55);
        return header;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::HeaderFormatter,
                               metadata,
                               out,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_HEADER_FORMATTER
