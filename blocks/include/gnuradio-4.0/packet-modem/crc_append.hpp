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
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cstdint>
#include <ranges>

namespace gr::packet_modem {

template <typename T = uint8_t, typename CrcType = uint64_t>
class CrcAppend : public gr::Block<CrcAppend<T, CrcType>>
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

public:
    unsigned _crc_num_bytes;
    // std::optional because it is constructed later
    std::optional<Crc<CrcType>> _crc;
    size_t _header_remaining;
    uint64_t _data_remaining;

private:
    void _set_crc()
    {
        _crc = Crc(
            num_bits, poly, initial_value, final_xor, input_reflected, result_reflected);
        _crc_num_bytes = num_bits / 8;
    }

public:
    gr::PortIn<uint8_t> in;
    // this port is Async because the scheduler doesn't understand that the
    // crc_append usually produces a few more output items than input items
    // consumed
    gr::PortOut<uint8_t, gr::Async> out;
    // These defaults correspond to CRC-32, which is also the default in the GNU
    // Radio 3.10 block
    unsigned num_bits = 32;
    CrcType poly = 0x4C11DB7;
    CrcType initial_value = 0xFFFFFFFF;
    CrcType final_xor = 0xFFFFFFFF;
    bool input_reflected = true;
    bool result_reflected = true;
    bool swap_endianness = false;
    uint64_t skip_header_bytes = 0;
    std::string packet_len_tag_key = "packet_len";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (num_bits % 8 != 0) {
            throw gr::exception("CRC number of bits must be a multiple of 8");
        }
        _set_crc();
    }

    void start()
    {
        _data_remaining = 0;
        _set_crc();
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "_data_remaining = {}, input_tags_present() = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _data_remaining,
                     this->input_tags_present());
#endif
        if (_data_remaining == 0) {
            if (inSpan.size() == 0) {
                if (!inSpan.consume(0)) {
                    throw gr::exception("consume failed");
                }
                outSpan.publish(0);
                return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
            }
            // Fetch the packet length tag to determine the length of the packet.
            auto not_found_error = [this]() {
                this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                       "expected packet-length tag not found");
                this->requestStop();
                return gr::work::Status::ERROR;
            };
            if (!this->input_tags_present()) {
                return not_found_error();
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(packet_len_tag_key)) {
                return not_found_error();
            }
            _data_remaining = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
            _header_remaining = skip_header_bytes;
            if (_header_remaining >= _data_remaining) {
                fmt::println("{} WARNING: received packet shorter than header "
                             "(length {})",
                             this->name,
                             _data_remaining);
            }
            _crc->initialize();
            // modify packet_len tag
            tag.map[packet_len_tag_key] = pmtv::pmt(_data_remaining + _crc_num_bytes);
            out.publishTag(tag.map, 0);
        } else if (_data_remaining > 0 && this->input_tags_present()) {
            // Propagate other data tags
            auto tag = this->mergedInputTag();
            out.publishTag(tag.map, 0);
        }

        size_t consumed = 0;
        size_t published = 0;
        auto out_data = outSpan.begin();

        if (_data_remaining > 0) {
            auto to_consume =
                std::min({ _data_remaining, inSpan.size(), outSpan.size() });
            // Do not consume all the data if there is no space in the output to
            // put the full CRC. This avoids getting the block in a stuck state
            // where it can produce output for free (the CRC), but the scheduler
            // doesn't call it.
            if (to_consume + _crc_num_bytes > outSpan.size()) {
                --to_consume;
            }
            _crc->update(inSpan | std::views::take(static_cast<ssize_t>(to_consume)) |
                         std::views::drop(_header_remaining));
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(to_consume), out_data);
            consumed += to_consume;
            published += to_consume;
            out_data += static_cast<ssize_t>(to_consume);
            _header_remaining -= std::min(_header_remaining, to_consume);
            _data_remaining -= to_consume;
        }

        if (_data_remaining == 0) {
            const auto crc = _crc->finalize();
            if (swap_endianness) {
                for (size_t j = 0; j < _crc_num_bytes; ++j) {
                    *out_data++ = (crc >> (8 * j)) & 0xff;
                }
            } else {
                for (size_t j = 0; j < _crc_num_bytes; ++j) {
                    *out_data++ = (crc >> (8 * (_crc_num_bytes - 1 - j))) & 0xff;
                }
            }
            published += _crc_num_bytes;
        }

        if (!inSpan.consume(consumed)) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(published);
#ifdef TRACE
        fmt::println("{}::processBulk() consume = {}, publish = {}",
                     this->name,
                     consumed,
                     published);
#endif

        if (published == 0) {
            return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }
        return gr::work::Status::OK;
    }
};

template <typename CrcType>
class CrcAppend<Pdu<uint8_t>, CrcType>
    : public gr::Block<CrcAppend<Pdu<uint8_t>, CrcType>>
{
public:
    using Description = CrcAppend<uint8_t, CrcType>::Description;

public:
    unsigned _crc_num_bytes;
    // std::optional because it is constructed later
    std::optional<Crc<CrcType>> _crc;

private:
    void _set_crc()
    {
        _crc = Crc(
            num_bits, poly, initial_value, final_xor, input_reflected, result_reflected);
        _crc_num_bytes = num_bits / 8;
    }

public:
    gr::PortIn<Pdu<uint8_t>> in;
    gr::PortOut<Pdu<uint8_t>> out;
    // These defaults correspond to CRC-32, which is also the default in the GNU
    // Radio 3.10 block
    unsigned num_bits = 32;
    CrcType poly = 0x4C11DB7;
    CrcType initial_value = 0xFFFFFFFF;
    CrcType final_xor = 0xFFFFFFFF;
    bool input_reflected = true;
    bool result_reflected = true;
    bool swap_endianness = false;
    uint64_t skip_header_bytes = 0;
    std::string packet_len_tag_key = "packet_len";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (num_bits % 8 != 0) {
            throw gr::exception("CRC number of bits must be a multiple of 8");
        }
        _set_crc();
    }

    void start() { _set_crc(); }

    [[nodiscard]] Pdu<uint8_t> processOne(const Pdu<uint8_t>& packet)
    {
        if (packet.data.size() <= skip_header_bytes) {
            fmt::println("{} WARNING: received packet shorter than header "
                         "(length {})",
                         this->name,
                         packet.data.size());
        }
        const auto crc = _crc->compute(packet.data | std::views::drop(skip_header_bytes));
        Pdu<uint8_t> out_packet = packet;
        if (swap_endianness) {
            for (size_t crc_remaining = _crc_num_bytes; crc_remaining > 0;
                 --crc_remaining) {
                out_packet.data.push_back(
                    (crc >> (8 * (_crc_num_bytes - crc_remaining))) & 0xff);
            }
        } else {
            for (size_t crc_remaining = _crc_num_bytes; crc_remaining > 0;
                 --crc_remaining) {
                out_packet.data.push_back((crc >> (8 * (crc_remaining - 1))) & 0xff);
            }
        }
        return out_packet;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::CrcAppend,
                               in,
                               out,
                               num_bits,
                               poly,
                               initial_value,
                               final_xor,
                               input_reflected,
                               result_reflected,
                               swap_endianness,
                               skip_header_bytes,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_CRC_APPEND
