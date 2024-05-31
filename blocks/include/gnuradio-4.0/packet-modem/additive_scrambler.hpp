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

The LFSR is defined by the `mask`, `seed` and `length` properties, which have
the same meaning as in GNU Radio 3.10. The LFSR can optionally be reset when a
`count` of scrambled symbols is reached (if `count == 0` this is disabled) or
when a tag with a particular key appears in the input (if `reset_tag_key == ""`
this is disabled).

Unlike the GNU Radio 3.10 block, this block does not support scrambling multiple
bits per byte. In the hard symbols case the input should be unpacked.

)"">;

public:
    uint64_t _reg;
    uint64_t _current_count;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    // The defaults for mask, seed and length correspond to a particular
    // scrambler implementation (the same as in GNU Radio 3.10)
    uint64_t mask = 0x8a;
    uint64_t seed = 0x7f;
    uint64_t length = 7;
    uint64_t count = 0;
    std::string reset_tag_key = "";

    static constexpr bool is_hard_symbol = std::is_same<T, uint8_t>();

    void start() { reset_lfsr(); }

    void reset_lfsr()
    {
        _reg = seed;
        _current_count = 0;
    }

    [[nodiscard]] constexpr T processOne(T a) noexcept
    {
        if ((!reset_tag_key.empty() && this->input_tags_present() &&
             this->mergedInputTag().map.contains(reset_tag_key)) ||
            (count != 0 && _current_count == count)) {
            reset_lfsr();
        }
        const uint8_t lfsr_bit = _reg & 1;
        const uint64_t shift_in = static_cast<uint64_t>(__builtin_parityl(_reg & mask));
        _reg = (shift_in << length) | (_reg >> 1);
        ++_current_count;

        if constexpr (is_hard_symbol) {
            return a ^ lfsr_bit;
        } else {
            return lfsr_bit ? -a : a;
        }
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::AdditiveScrambler,
                               in,
                               out,
                               mask,
                               seed,
                               length,
                               count,
                               reset_tag_key);

#endif // _GR4_PACKET_MODEM_ADDITIVE_SCRAMBLER
