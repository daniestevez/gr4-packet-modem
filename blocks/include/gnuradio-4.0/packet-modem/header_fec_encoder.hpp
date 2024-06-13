#ifndef _GR4_PACKET_MODEM_HEADER_FEC_ENCODER
#define _GR4_PACKET_MODEM_HEADER_FEC_ENCODER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>

namespace gr::packet_modem {

template <typename T = uint8_t>
class HeaderFecEncoder
    : public gr::Block<HeaderFecEncoder<T>, gr::ResamplingRatio<8U, 1U, true>>
{
public:
    using Description = Doc<R""(
@brief Header FEC Encoder.

Encodes a 32-bit header with a rate=1/8 encoder that is formed by the
concatenation of a (128, 32) LDPC code and a rate=1/2 repetition code. The input
of this block is in packed format as 8 bits per byte. The output is also in
packed format.

)"">;

public:
    static constexpr uint32_t _generator[] = {
        0x8ef9c844, 0x74ac6ee2, 0x3cfef71b, 0xb26263a9, 0x2dd63058, 0x007b3a60,
        0x31351305, 0xeaf6ef05, 0x05c7c06c, 0x14d54cea, 0x8b9a3a38, 0x014c7864,
        0x40f8d0fc, 0x61ef3bcd, 0xce500e2b, 0x9db2e7df, 0x011d14d6, 0x83164c42,
        0x766d4372, 0xead326fe, 0x919c7bc9, 0x5d7799a4, 0xedd6d997, 0xb5d68016,
        0x75109dd2, 0x87cf174e, 0xcc479aa7, 0x1db1a3a7, 0x8c927dfd, 0x5514181d,
        0x3f2d26cf, 0x4cb213a9, 0x4f8e715f, 0x1b975d94, 0xcaceb8d4, 0x9022fdb4,
        0x83d920b3, 0x9502c926, 0x24b815e6, 0xc51d5fb1, 0xf66c4372, 0x62e3b07b,
        0x7d6382a2, 0x3fe2683e, 0x26f13876, 0x7c471f48, 0x1da5b8a1, 0x6bbc09df,
        0xd6b6424e, 0xfbad49e5, 0xa00af367, 0xf3d0b974, 0x7d424b58, 0xb98860cf,
        0xbd51bb43, 0x908b1c3d, 0x414e7864, 0xe1ef3fcd, 0x75aba5ea, 0x6c79959f,
        0xf5109df2, 0x5a5f45d1, 0x84a8eb0d, 0xac33be50, 0x97b4a45c, 0x476a3987,
        0x81af4c18, 0x7f18b8c2, 0xd4a68d85, 0x784a836c, 0x3b409bd9, 0x4e836589,
        0x7e625eab, 0x6e7bc9f3, 0x3a9eac8d, 0xcddc8599, 0xa117efb1, 0x498f2a4c,
        0xa9f43e3d, 0x680a064d, 0x4e82093b, 0xf75157a4, 0x50947b04, 0xad5d2c65,
        0xd6cd382e, 0xbcf4047c, 0x916e95d0, 0xb00485ef, 0xa13e0f38, 0x7ff42423,
        0x20141b06, 0xde1bf63e, 0xf3ab831c, 0x049eb6ef, 0xe02623e7, 0x3cbfcfb0
    };

public:
    gr::PortIn<uint8_t> in;
    gr::PortOut<uint8_t> out;
    std::string packet_len_tag_key = "packet_len";

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
        const auto codewords = std::min(inSpan.size() / 4U, outSpan.size() / 32UZ);
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "codewords = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     codewords);
#endif

        if (codewords == 0) {
            if (!inSpan.consume(0)) {
                throw gr::exception("consume failed");
            }
            outSpan.publish(0);
            return inSpan.size() < 4U ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        if (!packet_len_tag_key.empty() && this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(packet_len_tag_key)) {
                // Adjust the packet_len tag value and overwrite the output tag
                // that is automatically propagated by the runtime.
                const auto packet_len = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
                tag.map[packet_len_tag_key] = pmtv::pmt(packet_len * 8U);
                out.publishTag(tag.map, 0);
            }
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();
        for (size_t j = 0; j < codewords; ++j) {
            // systematic coding
            std::copy_n(in_item, 4, out_item);

            // dense generator LDPC encoding
            const uint32_t info = (static_cast<uint32_t>(in_item[0]) << 24) |
                                  (static_cast<uint32_t>(in_item[1]) << 16) |
                                  (static_cast<uint32_t>(in_item[2]) << 8) |
                                  static_cast<uint32_t>(in_item[3]);
            for (int k = 0; k < 12; ++k) {
                uint8_t parity_bits = 0;
                for (int l = 0; l < 8; ++l) {
                    const uint32_t row = _generator[8 * k + l];
                    const uint8_t parity =
                        static_cast<uint8_t>(__builtin_parity(info & row));
                    parity_bits = static_cast<uint8_t>(parity_bits << 1) | parity;
                }
                out_item[4 + k] = parity_bits;
            }

            // repetition coding
            std::copy_n(out_item, 16, out_item + 16);

            in_item += 4;
            out_item += 32;
        }

        if (!inSpan.consume(codewords * 4U)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(codewords * 32U);

        return gr::work::Status::OK;
    }
};

template <>
class HeaderFecEncoder<Pdu<uint8_t>> : public gr::Block<HeaderFecEncoder<Pdu<uint8_t>>>
{
public:
    using Description = HeaderFecEncoder<uint8_t>::Description;

public:
    gr::PortIn<Pdu<uint8_t>> in;
    gr::PortOut<Pdu<uint8_t>> out;
    std::string packet_len_tag_key = "packet_len"; // unused

    [[nodiscard]] Pdu<uint8_t> processOne(const Pdu<uint8_t>& header)
    {
        Pdu<uint8_t> encoded;

        // systematic coding
        encoded.data = header.data;

        // dense generator LDPC encoding
        const uint32_t info = (static_cast<uint32_t>(header.data[0]) << 24) |
                              (static_cast<uint32_t>(header.data[1]) << 16) |
                              (static_cast<uint32_t>(header.data[2]) << 8) |
                              static_cast<uint32_t>(header.data[3]);
        for (int k = 0; k < 12; ++k) {
            uint8_t parity_bits = 0;
            for (int l = 0; l < 8; ++l) {
                const uint32_t row = HeaderFecEncoder<uint8_t>::_generator[8 * k + l];
                const uint8_t parity = static_cast<uint8_t>(__builtin_parity(info & row));
                parity_bits = static_cast<uint8_t>(parity_bits << 1) | parity;
            }
            encoded.data.push_back(parity_bits);
        }

        // repetition coding
        encoded.data.insert(
            encoded.data.end(), encoded.data.cbegin(), encoded.data.cend());

        return encoded;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::HeaderFecEncoder,
                               in,
                               out,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_HEADER_FEC_ENCODER
