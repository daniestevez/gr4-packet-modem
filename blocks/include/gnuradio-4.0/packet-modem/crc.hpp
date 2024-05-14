/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2022 Daniel Estevez <daniel@destevez.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_CRC
#define _GR4_PACKET_MODEM_CRC

#include <array>
#include <cstdint>
#include <ranges>

namespace gr::packet_modem {

template <class R>
concept byte_range =
    std::ranges::range<R> && std::same_as<std::ranges::range_value_t<R>, uint8_t>;

/*!
 * \brief Calculates a CRC
 *
 * \details
 * This class calculates a CRC with configurable parameters.
 * A table-driven byte-by-byte approach is used in the CRC
 * computation.
 */
template <typename T = uint64_t>
class Crc
{
private:
    std::array<T, 256> d_table;
    const unsigned d_num_bits;
    const T d_mask;
    const T d_initial_value;
    const T d_final_xor;
    const bool d_input_reflected;
    const bool d_result_reflected;
    T d_rem;

    constexpr T reflect(T word) const
    {
        T ret;
        ret = word & 1;
        for (unsigned i = 1; i < d_num_bits; ++i) {
            word >>= 1;
            ret = (ret << 1) | (word & 1);
        }
        return ret;
    }

public:
    /*!
     * \brief Construct a CRC calculator instance.
     *
     * \param num_bits CRC size in bits
     * \param poly CRC polynomial, in MSB-first notation
     * \param initial_value Initial register value
     * \param final_xor Final XOR value
     * \param input_reflected true if the input is LSB-first, false if not
     * \param result_reflected true if the output is LSB-first, false if not
     */
    Crc(unsigned num_bits,
        T poly,
        T initial_value = 0,
        T final_xor = 0,
        bool input_reflected = false,
        bool result_reflected = 0)
        : d_num_bits(num_bits),
          d_mask(num_bits == 8 * sizeof(T) ? ~T{ 0 } : (T{ 1 } << num_bits) - 1),
          d_initial_value(initial_value & d_mask),
          d_final_xor(final_xor & d_mask),
          d_input_reflected(input_reflected),
          d_result_reflected(result_reflected),
          d_rem(0)
    {
        if ((num_bits < 8) || (num_bits > 8 * sizeof(T))) {
            throw std::invalid_argument(
                "CRC number of bits must be between 8 and 8 * sizeof(T)");
        }

        d_table[0] = T{ 0 };
        if (d_input_reflected) {
            poly = reflect(poly);
            auto crc = T{ 1 };
            size_t i = 128UZ;
            do {
                if (crc & 1) {
                    crc = (crc >> 1) ^ poly;
                } else {
                    crc >>= 1;
                }
                for (size_t j = 0; j < 256UZ; j += 2UZ * i) {
                    d_table[i + j] = (crc ^ d_table[j]) & d_mask;
                }
                i >>= 1;
            } while (i > 0UZ);
        } else {
            const T msb = T{ 1 } << (num_bits - 1);
            auto crc = msb;
            size_t i = 1UZ;
            do {
                if (crc & msb) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
                for (size_t j = 0; j < i; ++j) {
                    d_table[i + j] = (crc ^ d_table[j]) & d_mask;
                }
                i <<= 1;
            } while (i < 256UZ);
        }
    }

    template <byte_range R>
    T compute(R&& data)
    {
        initialize();
        update(data);
        return finalize();
    }

    void initialize() { d_rem = d_initial_value; }

    template <byte_range R>
    void update(R&& data)
    {
        if (d_input_reflected) {
            for (const uint8_t& byte : data) {
                const uint8_t idx = (d_rem ^ byte) & 0xff;
                d_rem = d_table[idx] ^ (d_rem >> 8);
            }
        } else {
            for (const uint8_t& byte : data) {
                const uint8_t idx = ((d_rem >> (d_num_bits - 8)) ^ byte) & 0xff;
                d_rem = (d_table[idx] ^ (d_rem << 8)) & d_mask;
            }
        }
    }

    T finalize()
    {
        if (d_input_reflected != d_result_reflected) {
            d_rem = reflect(d_rem);
        }

        d_rem = d_rem ^ d_final_xor;
        return d_rem;
    }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_CRC
