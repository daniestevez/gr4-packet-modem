/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2008,2010,2012 Free Software Foundation, Inc.
 * Copyright 2024 Daniel Estevez <daniel@destevez.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_ADDITIVE_SCRAMBLER
#define _GR4_PACKET_MODEM_ADDITIVE_SCRAMBLER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <type_traits>
#include <algorithm>
#include <cstddef>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class AdditiveScrambler : public gr::Block<AdditiveScrambler<T>>
{
public:
    using Description = Doc<R""(
@brief Additive scrambler.

This block performs additive scrambling, in the same way as the GNU Radio 3.10
Additive Scrambler block. The block can either perform scrambling of hard
symbols when `T == uint8_t` by computing the XOR of the input and the LFSR bit,
or scrambling of soft symbols when `T != uint8_t`, by inverting the sign of the
input when the LFSR bit is 1.

The LFSR is defined by the `mask`, `seed` and `length` parameters, which have
the same meaning as in GNU Radio 3.10. The LFSR can optionally be reset when a
`count` of scrambled symbols is reached (if `count == 0` this is disabled) or
when a tag with a particular key appears in the input (if `reset_tag_key == ""`
this is disabled).

Unlike the GNU Radio 3.10 block, this block does not support scrambling multiple
bits per byte. In the hard symbols case the input should be unpacked.

)"">;

private:
    const uint64_t d_mask;
    const uint64_t d_seed;
    const unsigned d_length;
    const uint64_t d_count;
    const std::string d_reset_tag_key;
    uint64_t d_reg;
    uint64_t d_current_count = 0;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    static constexpr bool is_hard_symbol = std::is_same<T, uint8_t>();

    AdditiveScrambler(uint64_t mask,
                      uint64_t seed,
                      unsigned length,
                      uint64_t count = 0,
                      const std::string& reset_tag_key = "")
        : d_mask(mask),
          d_seed(seed),
          d_length(length),
          d_count(count),
          d_reset_tag_key(reset_tag_key),
          d_reg(seed)
    {
    }

    [[nodiscard]] constexpr T processOne(T a) noexcept
    {
        if ((!d_reset_tag_key.empty() && this->input_tags_present() &&
             this->mergedInputTag().map.contains(d_reset_tag_key)) ||
            (d_count != 0 && d_current_count == d_count)) {
            // reset LFSR
            d_reg = d_seed;
            d_current_count = 0;
        }
        const uint8_t lfsr_bit = d_reg & 1;
        const uint64_t shift_in =
            static_cast<uint64_t>(__builtin_parityl(d_reg & d_mask));
        d_reg = (shift_in << d_length) | (d_reg >> 1);
        ++d_current_count;

        if constexpr (is_hard_symbol) {
            return a ^ lfsr_bit;
        } else {
            return lfsr_bit ? -a : a;
        }
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::AdditiveScrambler, in, out);

#endif // _GR4_PACKET_MODEM_ADDITIVE_SCRAMBLER
