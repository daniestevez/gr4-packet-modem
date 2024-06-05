#ifndef _GR4_PACKET_MODEM_UNPACK_BITS
#define _GR4_PACKET_MODEM_UNPACK_BITS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/endianness.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
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

public:
    TIn _mask;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    size_t outputs_per_input = 1;
    TIn bits_per_output = 1;
    std::string packet_len_tag_key = "";

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (outputs_per_input <= 0) {
            throw gr::exception(fmt::format("outputs_per_input must be positive; got {}",
                                            outputs_per_input));
        }
        if (bits_per_output <= 0) {
            throw gr::exception(
                fmt::format("bits_per_output must be positive; got {}", bits_per_output));
        }
        _mask = static_cast<TIn>(TIn{ 1 } << bits_per_output) - TIn{ 1 };
        // set resampling ratio for the scheduler
        this->numerator = outputs_per_input;
        this->denominator = 1;
    }

    static constexpr Endianness kEndianness = endianness;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        assert(inSpan.size() == outSpan.size() / outputs_per_input);
        assert(inSpan.size() > 0);
        if (!packet_len_tag_key.empty() && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(packet_len_tag_key)) {
                // Adjust the packet_len tag value and overwrite the output tag
                // that is automatically propagated by the runtime.
                const auto packet_len = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
                tag.map[packet_len_tag_key] = pmtv::pmt(packet_len * outputs_per_input);
                out.publishTag(tag.map, 0);
            }
        }

        auto out_item = outSpan.begin();
        for (auto& in_item : inSpan) {
            if constexpr (kEndianness == Endianness::MSB) {
                TIn shift = bits_per_output * static_cast<TIn>(outputs_per_input - 1U);
                for (size_t j = 0; j < outputs_per_input; ++j) {
                    *out_item++ = static_cast<TOut>((in_item >> shift) & _mask);
                    shift -= bits_per_output;
                }
            } else {
                static_assert(kEndianness == Endianness::LSB);
                TIn item = in_item;
                for (size_t j = 0; j < outputs_per_input; ++j) {
                    *out_item++ = static_cast<TOut>(item & _mask);
                    item >>= bits_per_output;
                }
            }
        }

        return gr::work::Status::OK;
    }
};

template <Endianness endianness, typename TIn, typename TOut>
class UnpackBits<endianness, Pdu<TIn>, Pdu<TOut>>
    : public gr::Block<UnpackBits<endianness, Pdu<TIn>, Pdu<TOut>>>
{
public:
    using Description = UnpackBits<endianness, TIn, TOut>::Description;

public:
    TIn _mask;

public:
    gr::PortIn<Pdu<TIn>> in;
    gr::PortOut<Pdu<TOut>> out;
    size_t outputs_per_input = 1;
    TIn bits_per_output = 1;
    std::string packet_len_tag_key = "";

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (outputs_per_input <= 0) {
            throw gr::exception(fmt::format("outputs_per_input must be positive; got {}",
                                            outputs_per_input));
        }
        if (bits_per_output <= 0) {
            throw gr::exception(
                fmt::format("bits_per_output must be positive; got {}", bits_per_output));
        }
        _mask = static_cast<TIn>(TIn{ 1 } << bits_per_output) - TIn{ 1 };
    }

    static constexpr Endianness kEndianness = endianness;

    [[nodiscard]] Pdu<TOut> processOne(const Pdu<TIn>& pdu)
    {
        Pdu<TOut> pdu_out;
        pdu_out.data.reserve(pdu.data.size() * outputs_per_input);
        pdu_out.tags.reserve(pdu.tags.size());

        for (const auto& in_item : pdu.data) {
            if constexpr (kEndianness == Endianness::MSB) {
                TIn shift = bits_per_output * static_cast<TIn>(outputs_per_input - 1U);
                for (size_t j = 0; j < outputs_per_input; ++j) {
                    pdu_out.data.push_back(static_cast<TOut>((in_item >> shift) & _mask));
                    shift -= bits_per_output;
                }
            } else {
                static_assert(kEndianness == Endianness::LSB);
                TIn item = in_item;
                for (size_t j = 0; j < outputs_per_input; ++j) {
                    pdu_out.data.push_back(static_cast<TOut>(item & _mask));
                    item >>= bits_per_output;
                }
            }
        }

        for (auto tag : pdu.tags) {
            tag.index *= outputs_per_input;
            pdu_out.tags.push_back(tag);
        }

        return pdu_out;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE_FULL((gr::packet_modem::Endianness endianness,
                                     typename TIn,
                                     typename TOut),
                                    (gr::packet_modem::UnpackBits<endianness, TIn, TOut>),
                                    in,
                                    out,
                                    outputs_per_input,
                                    bits_per_output,
                                    packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_UNPACK_BITS
