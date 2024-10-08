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

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    double mult = 1.0;
    std::string packet_len_tag_key = "packet_len";

    // this needs custom tag propagation because it overwrites tags
    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    [[nodiscard]] constexpr T processOne(T a) noexcept
    {
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(packet_len_tag_key)) {
                uint64_t packet_len = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
                tag.map[packet_len_tag_key] = pmtv::pmt(static_cast<uint64_t>(
                    std::round(mult * static_cast<double>(packet_len))));
            }
#ifdef TRACE
            fmt::println("{} publishing tag: map = {}", this->name, tag.map);
#endif
            out.publishTag(tag.map, 0);
        }
        return a;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::MultiplyPacketLenTag, in, out, mult, packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_MULTIPLY_PACKET_LEN_TAG
