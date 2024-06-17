#ifndef _GR4_PACKET_MODEM_PACK_BITS
#define _GR4_PACKET_MODEM_PACK_BITS

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
`uint64_t` and is divisible by `inputs_per_output`. Otherwise, the block returns
an error.

)"">;

public:
    TIn _mask = TIn{ 1 };

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    size_t inputs_per_output = 1;
    TIn bits_per_input = TIn{ 1 };
    std::string packet_len_tag_key = "";

    // this needs custom tag propagation because it overwrites tags
    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (inputs_per_output <= 0) {
            throw gr::exception(fmt::format("inputs_per_output must be positive; got {}",
                                            inputs_per_output));
        }
        if (bits_per_input <= 0) {
            throw gr::exception(
                fmt::format("bits_per_input must be positive; got {}", bits_per_input));
        }
        _mask = static_cast<TIn>(TIn{ 1 } << bits_per_input) - TIn{ 1 };
        // set resampling ratio for the scheduler
        this->numerator = 1;
        this->denominator = inputs_per_output;
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
        assert(inSpan.size() / inputs_per_output == outSpan.size());
        assert(outSpan.size() > 0);
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (!packet_len_tag_key.empty() && tag.map.contains(packet_len_tag_key)) {
                // Adjust the packet_len tag value and overwrite the output tag
                // that is automatically propagated by the runtime.
                const auto packet_len = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
                if (packet_len % inputs_per_output) {
                    this->emitErrorMessage(
                        fmt::format("{}::processBulk", this->name),
                        fmt::format("packet_len {} is not divisible by "
                                    "inputs_per_output {}",
                                    packet_len,
                                    inputs_per_output));
                    this->requestStop();
                    return gr::work::Status::ERROR;
                }
                tag.map[packet_len_tag_key] = pmtv::pmt(packet_len / inputs_per_output);
            }
            out.publishTag(tag.map);
        }

        auto in_item = inSpan.begin();
        for (auto& out_item : outSpan) {
            TOut join = TOut{ 0 };
            TOut shift = TOut{ 0 };
            for (size_t j = 0; j < inputs_per_output; ++j) {
                const TOut chunk = static_cast<TOut>(*in_item++) & _mask;
                if constexpr (kEndianness == Endianness::MSB) {
                    join = static_cast<TOut>(join << static_cast<TOut>(bits_per_input)) |
                           chunk;
                } else {
                    static_assert(kEndianness == Endianness::LSB);
                    join |= chunk << shift;
                    shift += static_cast<TOut>(bits_per_input);
                }
            }
            out_item = join;
        }

        return gr::work::Status::OK;
    }
};

template <Endianness endianness, typename TIn, typename TOut>
class PackBits<endianness, Pdu<TIn>, Pdu<TOut>>
    : public gr::Block<PackBits<endianness, Pdu<TIn>, Pdu<TOut>>>
{
public:
    using Description = PackBits<endianness, TIn, TOut>::Description;

public:
    TIn _mask = TIn{ 1 };

public:
    gr::PortIn<Pdu<TIn>> in;
    gr::PortOut<Pdu<TOut>> out;
    size_t inputs_per_output = 1;
    TIn bits_per_input = TIn{ 1 };
    std::string packet_len_tag_key = "";

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (inputs_per_output <= 0) {
            throw gr::exception(fmt::format("inputs_per_output must be positive; got {}",
                                            inputs_per_output));
        }
        if (bits_per_input <= 0) {
            throw gr::exception(
                fmt::format("bits_per_input must be positive; got {}", bits_per_input));
        }
        _mask = static_cast<TIn>(TIn{ 1 } << bits_per_input) - TIn{ 1 };
    }

    static constexpr Endianness kEndianness = endianness;

    [[nodiscard]] Pdu<TOut> processOne(const Pdu<TIn>& pdu)
    {
        if (pdu.data.size() % inputs_per_output != 0) {
            throw gr::exception("input PDU size not divisible by inputs_per_output");
        }
        Pdu<TOut> pdu_out;
        pdu_out.data.reserve(pdu.data.size() / inputs_per_output);
        pdu_out.tags.reserve(pdu.tags.size());

        auto in_item = pdu.data.cbegin();
        while (in_item != pdu.data.cend()) {
            TOut join = TOut{ 0 };
            TOut shift = TOut{ 0 };
            for (size_t j = 0; j < inputs_per_output; ++j) {
                const TOut chunk = static_cast<TOut>(*in_item++) & _mask;
                if constexpr (kEndianness == Endianness::MSB) {
                    join = static_cast<TOut>(join << static_cast<TOut>(bits_per_input)) |
                           chunk;
                } else {
                    static_assert(kEndianness == Endianness::LSB);
                    join |= chunk << shift;
                    shift += static_cast<TOut>(bits_per_input);
                }
            }
            pdu_out.data.push_back(join);
        }

        for (auto tag : pdu.tags) {
            tag.index /= inputs_per_output;
            pdu_out.tags.push_back(tag);
        }

        return pdu_out;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE_FULL((gr::packet_modem::Endianness endianness,
                                     typename TIn,
                                     typename TOut),
                                    (gr::packet_modem::PackBits<endianness, TIn, TOut>),
                                    in,
                                    out,
                                    inputs_per_output,
                                    bits_per_input,
                                    packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_PACK_BITS
