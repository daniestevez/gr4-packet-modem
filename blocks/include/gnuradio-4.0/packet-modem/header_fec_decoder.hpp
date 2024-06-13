#ifndef _GR4_PACKET_MODEM_HEADER_FEC_DECODER
#define _GR4_PACKET_MODEM_HEADER_FEC_DECODER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <array>

namespace gr::packet_modem {

template <typename TIn = float, typename TOut = uint8_t>
class HeaderFecDecoder
    : public gr::Block<HeaderFecDecoder<TIn, TOut>, gr::ResamplingRatio<1U, 64U, true>>
{
public:
    using Description = Doc<R""(
@brief Header FEC Decoder.

Decodes the FEC of a header. See HeaderFecEncoder for details about the header
FEC. The input of this block is LLRs (soft symbols), with the conventions that
a positive LLR represents that the bit 0 is more likely. The output is the
decoded header packed as 8 bits per byte.

)"">;

private:
    static constexpr size_t header_num_llrs = 256;
    static constexpr size_t header_ldpc_n = 128;
    static constexpr size_t header_num_bytes = 4;

public:
    std::array<TIn, header_ldpc_n> _llrs;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        const auto codewords =
            std::min(inSpan.size() / header_num_llrs, outSpan.size() / header_num_bytes);
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "codewords = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     codewords);
#endif

        if (codewords == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return inSpan.size() < header_num_llrs
                       ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                       : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        for (size_t j = 0; j < codewords; ++j) {
            // accumulate LLRs for repetition coding
            std::copy_n(in_item, header_ldpc_n, _llrs.begin());
            for (size_t k = 0; k < header_ldpc_n; ++k) {
                _llrs[k] += in_item[static_cast<ssize_t>(header_ldpc_n + k)];
            }
            in_item += header_num_llrs;

            // TODO: perform LDPC decoding

            // Slice systematic bits and pack
            for (size_t k = 0; k < header_num_bytes; ++k) {
                TOut byte = 0;
                for (size_t n = 0; n < 8; ++n) {
                    byte = static_cast<TOut>(byte << 1) | TOut { _llrs[8 * k + n] < 0 };
                }
                *out_item++ = byte;
            }
        }

        if (!inSpan.consume(codewords * header_num_llrs)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(codewords * header_num_bytes);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::HeaderFecDecoder, in, out);

#endif // _GR4_PACKET_MODEM_HEADER_FEC_DECODER
