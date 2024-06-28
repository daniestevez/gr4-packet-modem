#ifndef _GR4_PACKET_MODEM_HEADER_PARSER
#define _GR4_PACKET_MODEM_HEADER_PARSER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <pmtv/pmt.hpp>

namespace gr::packet_modem {

static constexpr size_t HEADER_PARSER_HEADER_LEN = 4U;

template <typename T = uint8_t>
class HeaderParser : public gr::Block<HeaderParser<T>,
                                      gr::Resampling<HEADER_PARSER_HEADER_LEN, 1U, true>>
{
public:
    using Description = Doc<R""(
@brief Header Parser. Parses a header into a metadata message.

This block receives headers packed as 8 bits per byte in the input port and
for each header it generates a metadata message with the parsed values from
the header in the metadata port.

See Header Formatter for details about the header format.

)"">;

private:
    static constexpr size_t HEADER_LEN = HEADER_PARSER_HEADER_LEN;

public:
    gr::PortIn<uint8_t> in;
    gr::PortOut<gr::Message> metadata;

public:
    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        assert(inSpan.size() == outSpan.size() * HEADER_LEN);
        assert(inSpan.size() > 0);
        auto header = inSpan.begin();
        for (auto& meta : outSpan) {
#ifdef TRACE
            fmt::println("{} header = {:02x} {:02x} {:02x} {:02x}",
                         this->name,
                         header[0],
                         header[1],
                         header[2],
                         header[3]);
#endif
            bool valid = true;
            if (this->input_tags_present() &&
                this->mergedInputTag().map.contains("invalid_header")) {
#ifdef TRACE
                fmt::println("{} LDPC decoder error", this->name);
#endif
                // LDPC decoder error
                valid = false;
            }
            const uint64_t packet_length = (static_cast<uint64_t>(header[0]) << 8) |
                                           static_cast<uint64_t>(header[1]);
            if (packet_length == 0) {
                valid = false;
            }
            const uint8_t modcod_field = header[2];
            if (modcod_field != 0x00) {
                // the only modcod field supported at the moment is 0x00, which is QPSK
                // uncoded
                valid = false;
            }
            gr::Message msg;
            if (valid) {
                msg.data = gr::property_map{ { "packet_length", packet_length },
                                             { "constellation", "QPSK" } };
            } else {
                msg.data = gr::property_map{ { "invalid_header", pmtv::pmt_null() } };
            }
#ifdef TRACE
            fmt::println("{} sending message data {}", this->name, msg.data);
#endif
            meta = std::move(msg);

            header += HEADER_LEN;
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::HeaderParser, in, metadata);

#endif // _GR4_PACKET_MODEM_HEADER_PARSER
