/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2007,2012,2016 Free Software Foundation, Inc.
 * Copyright 2008,2010,2012 Free Software Foundation, Inc.
 * Copyright 2024 Daniel Estevez <daniel@destevez.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_GLFSR_SOURCE
#define _GR4_PACKET_MODEM_GLFSR_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T = uint8_t>
class GlfsrSource : public gr::Block<GlfsrSource<T>>
{
public:
    using Description = Doc<R""(
@brief GLFSR Source.

This block uses a GLFSR of the degree indicated in the constructor to generate a
pseudo-random sequence of bits.

)"">;

private:
    uint64_t d_mask;
    uint64_t d_reg;

public:
    gr::PortOut<T> out;

    static constexpr uint64_t polynomial_masks[] = {
        0x00000000,
        0x00000001, // x^1 + 1
        0x00000003, // x^2 + x^1 + 1
        0x00000005, // x^3 + x^1 + 1
        0x00000009, // x^4 + x^1 + 1
        0x00000012, // x^5 + x^2 + 1
        0x00000021, // x^6 + x^1 + 1
        0x00000041, // x^7 + x^1 + 1
        0x0000008E, // x^8 + x^4 + x^3 + x^2 + 1
        0x00000108, // x^9 + x^4 + 1
        0x00000204, // x^10 + x^4 + 1
        0x00000402, // x^11 + x^2 + 1
        0x00000829, // x^12 + x^6 + x^4 + x^1 + 1
        0x0000100D, // x^13 + x^4 + x^3 + x^1 + 1
        0x00002015, // x^14 + x^5 + x^3 + x^1 + 1
        0x00004001, // x^15 + x^1 + 1
        0x00008016, // x^16 + x^5 + x^3 + x^2 + 1
        0x00010004, // x^17 + x^3 + 1
        0x00020013, // x^18 + x^5 + x^2 + x^1 + 1
        0x00040013, // x^19 + x^5 + x^2 + x^1 + 1
        0x00080004, // x^20 + x^3 + 1
        0x00100002, // x^21 + x^2 + 1
        0x00200001, // x^22 + x^1 + 1
        0x00400010, // x^23 + x^5 + 1
        0x0080000D, // x^24 + x^4 + x^3 + x^1 + 1
        0x01000004, // x^25 + x^3 + 1
        0x02000023, // x^26 + x^6 + x^2 + x^1 + 1
        0x04000013, // x^27 + x^5 + x^2 + x^1 + 1
        0x08000004, // x^28 + x^3 + 1
        0x10000002, // x^29 + x^2 + 1
        0x20000029, // x^30 + x^4 + x^1 + 1
        0x40000004, // x^31 + x^3 + 1
        0x80000057  // x^32 + x^7 + x^5 + x^3 + x^2 + x^1 + 1
    };

    GlfsrSource(size_t degree, uint64_t seed = 0x1) : d_reg(seed)
    {
        if (degree > 32) {
            throw std::invalid_argument(
                fmt::format("{} degree {} too large", this->name, degree));
        }
        d_mask = polynomial_masks[degree];
    }

    [[nodiscard]] constexpr T processOne() noexcept
    {
        const uint8_t bit = d_reg & 1;
        d_reg >>= 1;
        if (bit) {
            d_reg ^= d_mask;
        }
        return T{ bit };
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::GlfsrSource, out);

#endif // _GR4_PACKET_MODEM_GLFSR_SOURCE
