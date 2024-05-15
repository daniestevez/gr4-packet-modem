#ifndef _GR4_PACKET_MODEM_UNPACK_BITS
#define _GR4_PACKET_MODEM_UNPACK_BITS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/endianness.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>
#include <stdexcept>

namespace gr::packet_modem {

template <Endianness endianness = Endianness::MSB,
          typename TIn = uint8_t,
          typename TOut = uint8_t>
class UnpackBits
    : public gr::Block<UnpackBits<endianness, TIn, TOut>, gr::ResamplingRatio<>>
{
public:
    using Description = Doc<R""(
@brief Unpack Bits. Unpacks k*n-bit nibbles to k nibbles of n.

This block performs the opposite function of Pack Bits. It assumes that the
input items are formed by nibbles of `bits_per_output * outputs_per_input` bits
LSBs of the item. It unpacks these nibbles into `outputs_per_input` output
items, each of them containing `bits_per_output` bits of the corresponding input
items. The output nibbles are placed on the LSBs of the output items. The order
in which the input nibbles are unpacked in the output is given by the
`Endianness` template parameters. If the order is `MSB`, then the
`bits_per_output` MSBs of the input nibble are placed on the first output
item. If the order is `LSB`, then the `bits_per_output` LSBs of the input nibble
are placed on the output item. The bits within each `bits_per_output`-bit nibble
are not rearranged regardless of the `Endianness` used.

If the input items contain data in the bits above the `bits_per_input *
outputs_per_input` LSBs, this data is discarded.

As an example, using this block with `outputs_per_input = 8` and
`bits_per_output = 1` serves to convert a packed 8-bits-per-byte input into an
unpacked one-bit-per-byte output.

By default this block operates on `uint8_t` input and output items, but it can
use larger integers. This is required when the input or output nibbles have more
than 8 bits. For example, using `TIn = uint16_t`, `outputs_per_input = 10` and
`bits_per_output = 1` can be used to unpack 10 bits from a `uint16_t` input to a
one-bit-per-byte `uint8_t` output. This could be used to unpack the bits in the
symbols of a 1024QAM constellation.

The block can optionally adjust the length of packet-length tags. If a non-empty
string is supplied in the `packet_length_tag_key` constructor argument, the
value of tags with that key will be multiplied by `outputs_per_input` in the
output. It is assumed that the value of these tags can be converted to
`uint64_t`. Otherwise, the block throws an exception.

)"">;

private:
    const size_t d_outputs_per_input;
    const TIn d_shift;
    const TIn d_mask;
    const std::string d_packet_len_tag_key;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    UnpackBits(size_t outputs_per_input,
               TIn bits_per_output = 1,
               const std::string& packet_len_tag_key = "")
        : d_outputs_per_input(outputs_per_input),
          d_shift(bits_per_output),
          d_mask(static_cast<TIn>(TIn{ 1 } << bits_per_output) - TIn{ 1 }),
          d_packet_len_tag_key(packet_len_tag_key)
    {
        if (bits_per_output <= 0) {
            throw std::invalid_argument(
                fmt::format("[UnpackBits] bits_per_output must be positive; got {}",
                            bits_per_output));
        }
        // set resampling ratio for the scheduler
        this->numerator = outputs_per_input;
        this->denominator = 1;
    }

    static constexpr Endianness kEndianness = endianness;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        const auto to_consume =
            std::min(inSpan.size(), outSpan.size() / d_outputs_per_input);
#ifdef TRACE
        std::print("UnpackBits::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                   "to_consume = {}\n",
                   inSpan.size(),
                   outSpan.size(),
                   to_consume);
#endif

        if (to_consume == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return inSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        if (!d_packet_len_tag_key.empty() && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(d_packet_len_tag_key)) {
                // Adjust the packet_len tag value and overwrite the output tag
                // that is automatically propagated by the runtime.
                const auto packet_len =
                    pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
                tag.map[d_packet_len_tag_key] =
                    pmtv::pmt(packet_len * d_outputs_per_input);
                out.publishTag(tag.map, 0);
            }
        }

        auto out_item = outSpan.begin();
        for (auto& in_item : inSpan | std::views::take(to_consume)) {
            if constexpr (kEndianness == Endianness::MSB) {
                TIn shift = d_shift * static_cast<TIn>(d_outputs_per_input - 1U);
                for (size_t j = 0; j < d_outputs_per_input; ++j) {
                    *out_item++ = static_cast<TOut>((in_item >> shift) & d_mask);
                    shift -= d_shift;
                }
            } else {
                static_assert(kEndianness == Endianness::LSB);
                TIn item = in_item;
                for (size_t j = 0; j < d_outputs_per_input; ++j) {
                    *out_item++ = static_cast<TOut>(item & d_mask);
                    item >>= d_shift;
                }
            }
        }

        std::ignore = inSpan.consume(to_consume);
        outSpan.publish(to_consume * d_outputs_per_input);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE_FULL((gr::packet_modem::Endianness endianness,
                                     typename TIn,
                                     typename TOut),
                                    (gr::packet_modem::UnpackBits<endianness, TIn, TOut>),
                                    in,
                                    out);

#endif // _GR4_PACKET_MODEM_UNPACK_BITS
