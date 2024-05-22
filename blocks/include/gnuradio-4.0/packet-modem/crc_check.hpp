/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2022 Daniel Estevez <daniel@destevez.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_CRC_CHECK
#define _GR4_PACKET_MODEM_CRC_CHECK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/crc.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cstdint>
#include <ranges>

namespace gr::packet_modem {

template <typename CrcType = uint64_t>
class CrcCheck : public gr::Block<CrcCheck<CrcType>>
{
public:
    using Description = Doc<R""(
@brief CRC Check. Drops packets with wrong CRC.

The input of this block is a stream of `uint8_t` items forming packets delimited
by a packet-length tag that is present in the first item of the packet and whose
value indicates the length of the packet. The output has the same format.

For each packet, the block calculates its CRC and compares it to the CRC present
at the end of the packet. If the CRC matches, the packet is copied to the
output. If the CRC does not match or if the packet is too short to contain a
CRC, the packet is dropped.

The block can optionally work with CRCs with swapped byte-endianness by using
the `swap_endinanness` parameter, and skip a fixed number of "header bytes" in
the CRC calculation with the `skip_header_bytes` parameter. Additionally, the
CRC can be discarded in the output packets with the `discard_crc` parameter.

TODO: This block does not handle tags that appear mid-packet. This is because
the scheduler calls `processBulk()` once for every tag, but this block requires
the whole packet to be present in the input at once.

)"">;

private:
    const unsigned d_crc_num_bytes;
    const bool d_swap_endianness;
    Crc<CrcType> d_crc;
    const bool d_discard_crc;
    const size_t d_header_bytes;
    const std::string d_packet_len_tag_key;
    uint64_t d_packet_len;
    gr::Tag d_tag;

public:
    gr::PortIn<uint8_t> in;
    gr::PortOut<uint8_t> out;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    CrcCheck(unsigned num_bits,
             CrcType poly,
             CrcType initial_value = 0,
             CrcType final_xor = 0,
             bool input_reflected = false,
             bool result_reflected = false,
             bool swap_endianness = false,
             bool discard_crc = false,
             size_t skip_header_bytes = 0,
             const std::string& packet_len_tag_key = "packet_len")
        : d_crc_num_bytes(num_bits / 8),
          d_swap_endianness(swap_endianness),
          d_crc(num_bits,
                poly,
                initial_value,
                final_xor,
                input_reflected,
                result_reflected),
          d_discard_crc(discard_crc),
          d_header_bytes(skip_header_bytes),
          d_packet_len_tag_key(packet_len_tag_key),
          d_packet_len(0)
    {
        if (num_bits % 8 != 0) {
            throw std::invalid_argument(
                fmt::format("{} CRC number of bits must be a multiple of 8", this->name));
        }
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "input_tags_present() = {}, d_tag.map = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     this->input_tags_present(),
                     d_tag.map);
#endif
        if (d_packet_len == 0) {
            // Tags can only be fetched in one processBulk() call. They
            // disappear in the next call, even if the input is not consumed. We
            // store the value in case we need it.

            // Fetch the packet length tag to determine the length of the packet.
            static constexpr auto not_found =
                "[CrcCheck] expected packet-length tag not found";
            if (!this->input_tags_present()) {
                throw std::runtime_error(not_found);
            }
            d_tag = this->mergedInputTag();
            if (!d_tag.map.contains(d_packet_len_tag_key)) {
                throw std::runtime_error(not_found);
            }
            d_packet_len = pmtv::cast<uint64_t>(d_tag.map[d_packet_len_tag_key]);
            if (d_packet_len == 0) {
                throw std::runtime_error("[CrcCheck] received packet_len = 0");
            }
        }

        if (inSpan.size() < d_packet_len) {
            // we need the whole packet to be present in the input at once to
            // decide its fate (it can be dropped or passed to the output)
#ifdef TRACE
            fmt::println("{}::processBulk() -> INSUFFICIENT_INPUT_ITEMS", this->name);
#endif
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        if (d_packet_len <= d_crc_num_bytes) {
            // the packet is too short; drop it
            fmt::println(
                "{} packet is too short (length {}); dropping", this->name, d_packet_len);
            std::ignore = inSpan.consume(d_packet_len);
            outSpan.publish(0);
            d_packet_len = 0;
            return gr::work::Status::OK;
        }

        const auto payload_size = d_packet_len - d_crc_num_bytes;
        const auto crc_computed =
            d_crc.compute(inSpan | std::views::take(static_cast<ssize_t>(payload_size)) |
                          std::views::drop(d_header_bytes));

        // Read CRC from packet
        CrcType crc_packet = CrcType{ 0 };
        if (d_swap_endianness) {
            for (size_t i = d_packet_len - 1; i >= payload_size; --i) {
                crc_packet <<= 8;
                crc_packet |= inSpan[i];
            }
        } else {
            for (size_t i = payload_size; i < d_packet_len; ++i) {
                crc_packet <<= 8;
                crc_packet |= inSpan[i];
            }
        }

        const bool crc_ok = crc_packet == crc_computed;

        if (crc_ok) {
            const size_t output_size = d_discard_crc ? payload_size : d_packet_len;
            if (outSpan.size() < output_size) {
#ifdef TRACE
                fmt::println("{}::processBulk() -> INSUFFICIENT_OUTPUT_ITEMS",
                             this->name);
#endif
                outSpan.publish(0);
                std::ignore = inSpan.consume(0);
                return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
            }
            // modify packet_len tag
            d_tag.map[d_packet_len_tag_key] = pmtv::pmt(output_size);
            out.publishTag(d_tag.map, 0);
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(output_size), outSpan.begin());
            outSpan.publish(output_size);
        } else {
            outSpan.publish(0);
        }

        std::ignore = inSpan.consume(d_packet_len);
        d_packet_len = 0;

#ifdef TRACE
        fmt::println("{}::processBulk() -> OK", this->name);
#endif

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::CrcCheck, in, out);

#endif // _GR4_PACKET_MODEM_CRC_CHECK
