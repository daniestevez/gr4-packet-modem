/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2022 Daniel Estevez <daniel@destevez.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_CRC_APPEND
#define _GR4_PACKET_MODEM_CRC_APPEND

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/crc.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cstdint>
#include <ranges>

namespace gr::packet_modem {

template <typename CrcType = uint64_t>
class CrcAppend : public gr::Block<CrcAppend<CrcType>>
{
public:
    using Description = Doc<R""(
@brief CRC Append. Calculates and appends a CRC at the end of a packet.

The input of this block is a stream of `uint8_t` items forming packets delimited
by a packet-length tag that is present in the first item of the packet and whose
value indicates the length of the packet. The output has the same format.

For each packet, the block calculates its CRC and appends it to the packet to
form the output packet. The CRC parameters can be configured in the block
constructor (see `<gnuradio-4.0/packet-modem/crc.hpp>` for the meaning of each
parameter). The block can optionally swap the byte-endianness of the CRC with
the `swap_endinanness` parameter and skip a fixed number of "header bytes" in
the CRC calculation with the `skip_header_bytes` parameter.

)"">;

private:
    const unsigned d_crc_num_bytes;
    const bool d_swap_endianness;
    Crc<CrcType> d_crc;
    const size_t d_header_bytes;
    const std::string d_packet_len_tag_key;
    size_t d_header_remaining;
    uint64_t d_data_remaining;
    unsigned d_crc_remaining;
    CrcType d_crc_rem;

public:
    gr::PortIn<uint8_t> in;
    gr::PortOut<uint8_t, gr::Async> out;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    CrcAppend(unsigned num_bits,
              CrcType poly,
              CrcType initial_value = 0,
              CrcType final_xor = 0,
              bool input_reflected = false,
              bool result_reflected = false,
              bool swap_endianness = false,
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
          d_header_bytes(skip_header_bytes),
          d_packet_len_tag_key(packet_len_tag_key),
          d_header_remaining(0),
          d_data_remaining(0),
          d_crc_remaining(0),
          d_crc_rem(0)
    {
        if (num_bits % 8 != 0) {
            throw std::invalid_argument("CRC number of bits must be a multiple of 8");
        }
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("CrcAppend::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "d_data_remaining = {}, d_crc_remaining = {}",
                     inSpan.size(),
                     outSpan.size(),
                     d_data_remaining,
                     d_crc_remaining);
#endif
        if (d_crc_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            static constexpr auto not_found =
                "[CrcAppend] expected packet-length tag not found\n";
            if (!this->input_tags_present()) {
                throw std::runtime_error(not_found);
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(d_packet_len_tag_key)) {
                throw std::runtime_error(not_found);
            }
            d_data_remaining = pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
            d_header_remaining = d_header_bytes;
            if (d_header_remaining >= d_data_remaining) {
                std::println("[CrcAppend] WARNING: received packet shorter than header "
                             "(length {})",
                             d_data_remaining);
            }
            d_crc_remaining = d_crc_num_bytes;
            d_crc.initialize();
            // modify packet_len tag
            tag.map[d_packet_len_tag_key] = pmtv::pmt(d_data_remaining + d_crc_remaining);
            out.publishTag(tag.map, 0);
        } else if (d_data_remaining > 0 && this->input_tags_present()) {
            // Propagate other data tags
            auto tag = this->mergedInputTag();
            out.publishTag(tag.map, 0);
        }

        size_t consumed = 0;
        size_t published = 0;
        auto out_data = outSpan.begin();

        if (d_data_remaining > 0) {
            const auto to_consume =
                std::min({ d_data_remaining, inSpan.size(), outSpan.size() });
            d_crc.update(inSpan | std::views::take(static_cast<ssize_t>(to_consume)) |
                         std::views::drop(d_header_remaining));
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(to_consume), out_data);
            consumed += to_consume;
            published += to_consume;
            out_data += static_cast<ssize_t>(to_consume);
            d_header_remaining -= std::min(d_header_remaining, to_consume);
            d_data_remaining -= to_consume;
            if (d_data_remaining == 0) {
                d_crc_rem = d_crc.finalize();
            }
        }

        if (d_data_remaining == 0) {
            assert(d_crc_remaining > 0);
            const auto to_publish =
                std::min(static_cast<size_t>(d_crc_remaining),
                         static_cast<size_t>(outSpan.size() - published));
            if (d_swap_endianness) {
                for (size_t j = 0; j < to_publish; ++j) {
                    *out_data++ =
                        (d_crc_rem >> (8 * (d_crc_num_bytes - d_crc_remaining))) & 0xff;
                    --d_crc_remaining;
                }
            } else {
                for (size_t j = 0; j < to_publish; ++j) {
                    *out_data++ = (d_crc_rem >> (8 * (d_crc_remaining - 1))) & 0xff;
                    --d_crc_remaining;
                }
            }
            published += to_publish;
        }

        std::ignore = inSpan.consume(consumed);
        outSpan.publish(published);
#ifdef TRACE
        fmt::println(
            "CrcAppend::processBulk() consume = {}, publish = {}", consumed, published);
#endif

        if (published == 0) {
            return d_data_remaining > 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                        : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::CrcAppend, in, out);

#endif // _GR4_PACKET_MODEM_CRC_APPEND
