#ifndef _GR4_PACKET_MODEM_PACK_BITS
#define _GR4_PACKET_MODEM_PACK_BITS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/endianness.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>
#include <stdexcept>

namespace gr::packet_modem {

template <Endianness endianness = Endianness::MSB,
          typename TIn = uint8_t,
          typename TOut = uint8_t>
class PackBits : public gr::Block<PackBits<endianness, TIn, TOut>, gr::ResamplingRatio<>>
{
public:
    using Description = Doc<R""(
@brief Pack Bits. Packs k nibbles of n bits into k*n-bit nibbles.

This block assumes that the input items are formed by nibbles of
`bits_per_input` bits placed in the LSBs of the item. It packs these nibbles in
`inputs_per_output` input items to form an output sample, which contains a
nibble of `bits_per_input * inputs_per_output` bits in which the nibbles in the
corresponding input items have been concatenated. The order of this
concatenation is given by the `Endianness` template parameter. If the order is
`MSB`, then the nibble in the first item is placed in the MSBs of the
concatenated nibble. If the order is `LSB`, then the nibble in the first item is
placed in the LSBs of the concatenated nibble. The bits within each input nibble
are not rearranged regardless of the `Endianness` used.

If the input items contain data in the bits above the `bits_per_input` LSBs,
this data is discarded.

As an example, using this block with `inputs_per_output = 8` and
`bits_per_input = 1` serves to convert an unpacked one-bit-per-byte input into a
packed 8-bits-per-byte output.

By default this block operates on `uint8_t` input and output items, but it can
use larger integers. This is required when the input or output nibbles have more
than 8 bits. For example, using `TOut = uint16_t`, `inputs_per_output = 10` and
`bits_per_input = 1` can be used to pack 10 bits from a one-bit-per-byte
`uint8_t` input into 10-bit nibbles in a `uint16_t` output.  This could be used
to feed a 1024QAM constellation modulator.

The block can optionally adjust the length of packet-length tags. If a non-empty
string is supplied in the `packet_length_tag_key` constructor argument, the
value of tags with that key will be dividied by `inputs_per_output` in the
output. It is assumed that the value of these tags can be converted to
`uint64_t` and is divisible by `inputs_per_output`. Otherwise, the block throws
an exception.

)"">;

private:
    const size_t d_inputs_per_output;
    const TOut d_shift;
    const TIn d_mask;
    const std::string d_packet_len_tag_key;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    PackBits(size_t inputs_per_output,
             TIn bits_per_input = 1,
             const std::string& packet_len_tag_key = "")
        : d_inputs_per_output(inputs_per_output),
          d_shift(bits_per_input),
          d_mask(static_cast<TIn>(TIn{ 1 } << bits_per_input) - TIn{ 1 }),
          d_packet_len_tag_key(packet_len_tag_key)
    {
        if (bits_per_input <= 0) {
            throw std::invalid_argument(fmt::format(
                "[PackBits] bits_per_input must be positive; got {}", bits_per_input));
        }
        // set resampling ratio for the scheduler
        this->numerator = 1;
        this->denominator = inputs_per_output;
    }

    static constexpr Endianness kEndianness = endianness;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        const auto to_publish =
            std::min(inSpan.size() / d_inputs_per_output, outSpan.size());
#ifdef TRACE
        std::print("PackBits::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                   "to_publish = {}\n",
                   inSpan.size(),
                   outSpan.size(),
                   to_publish);
#endif

        if (to_publish == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return outSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS
                                       : gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }

        if (!d_packet_len_tag_key.empty() && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(d_packet_len_tag_key)) {
                // Adjust the packet_len tag value and overwrite the output tag
                // that is automatically propagated by the runtime.
                const auto packet_len =
                    pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
                if (packet_len % d_inputs_per_output) {
                    throw std::runtime_error(
                        fmt::format("[PackBits] packet_len {} is not divisible by "
                                    "inputs_per_output {}",
                                    packet_len,
                                    d_inputs_per_output));
                }
                tag.map[d_packet_len_tag_key] =
                    pmtv::pmt(packet_len / d_inputs_per_output);
                out.publishTag(tag.map, 0);
            }
        }

        auto in_item = inSpan.begin();
        for (auto& out_item : outSpan | std::views::take(to_publish)) {
            TOut join = TOut{ 0 };
            TOut shift = TOut{ 0 };
            for (size_t j = 0; j < d_inputs_per_output; ++j) {
                const TOut chunk = static_cast<TOut>(*in_item++) & d_mask;
                if constexpr (kEndianness == Endianness::MSB) {
                    join = static_cast<TOut>(join << d_shift) | chunk;
                } else {
                    static_assert(kEndianness == Endianness::LSB);
                    join |= chunk << shift;
                    shift += d_shift;
                }
            }
            out_item = join;
        }

        std::ignore = inSpan.consume(to_publish * d_inputs_per_output);
        outSpan.publish(to_publish);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE_FULL((gr::packet_modem::Endianness endianness,
                                     typename TIn,
                                     typename TOut),
                                    (gr::packet_modem::PackBits<endianness, TIn, TOut>),
                                    in,
                                    out);

#endif // _GR4_PACKET_MODEM_PACK_BITS
