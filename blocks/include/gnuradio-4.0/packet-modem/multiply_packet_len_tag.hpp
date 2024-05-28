#ifndef _GR4_PACKET_MODEM_MULTIPLY_PACKET_LEN_TAG
#define _GR4_PACKET_MODEM_MULTIPLY_PACKET_LEN_TAG

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <pmtv/pmt.hpp>

namespace gr::packet_modem {

template <typename T>
class MultiplyPacketLenTag : public gr::Block<MultiplyPacketLenTag<T>>
{
public:
    using Description = Doc<R""(
@brief Multiply packet length tag.

This block multiplies packet length tags by a fixed given value.

)"">;

private:
    const double d_mult;
    const std::string d_packet_len_tag_key;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    MultiplyPacketLenTag(double mult,
                         const std::string& packet_len_tag_key = "packet_len")
        : d_mult(mult), d_packet_len_tag_key(packet_len_tag_key)
    {
    }

    [[nodiscard]] constexpr T processOne(T a) noexcept
    {
        if (this->input_tags_present()) {
            auto tags = this->mergedInputTag();
            if (tags.map.contains(d_packet_len_tag_key)) {
                uint64_t packet_len =
                    pmtv::cast<uint64_t>(tags.map[d_packet_len_tag_key]);
                tags.map[d_packet_len_tag_key] = pmtv::pmt(static_cast<uint64_t>(
                    std::round(d_mult * static_cast<double>(packet_len))));
                out.publishTag(tags.map, 0);
            }
        }
        return a;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::MultiplyPacketLenTag, in, out);

#endif // _GR4_PACKET_MODEM_MULTIPLY_PACKET_LEN_TAG