#ifndef _GR4_PACKET_MODEM_PACK_BITS
#define _GR4_PACKET_MODEM_PACK_BITS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/endianness.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <Endianness endianness = Endianness::MSB,
          typename TIn = uint8_t,
          typename TOut = uint8_t>
class PackBits : public gr::Block<PackBits<endianness, TIn, TOut>>
{
public:
    using Description = Doc<R""(
@brief Pack Bits.

TODO...

TODO: packet_len tag adjustment

)"">;

private:
    const size_t d_inputs_per_output;
    const TOut d_shift;
    const TIn d_mask;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    PackBits(size_t inputs_per_output, TIn bits_per_input = 1)
        : d_inputs_per_output(inputs_per_output),
          d_shift(bits_per_input),
          d_mask(static_cast<TIn>(TIn{ 1 } << bits_per_input) - TIn{ 1 })
    {
        if (bits_per_input <= 0) {
            throw std::invalid_argument(fmt::format(
                "[PackBits] bits_per_input must be positive; got {}", bits_per_input));
        }
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

        auto in_item = inSpan.begin();
        for (auto& out_item : outSpan | std::views::take(to_publish)) {
            TOut join = TOut{ 0 };
            TOut shift = TOut{ 0 };
            for (size_t j = 0; j < d_inputs_per_output; ++j) {
                const TOut chunk = static_cast<TOut>(*in_item++) & d_mask;
                if constexpr (kEndianness == Endianness::MSB) {
                    join = static_cast<TOut>(join << d_shift) | chunk;
                } else {
                    // LSB
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
