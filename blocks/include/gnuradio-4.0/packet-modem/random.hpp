/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2002,2015,2018 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * and also
 *
 *  Copyright 1997 Massachusetts Institute of Technology
 *
 *  Permission to use, copy, modify, distribute, and sell this software and its
 *  documentation for any purpose is hereby granted without fee, provided that
 *  the above copyright notice appear in all copies and that both that
 *  copyright notice and this permission notice appear in supporting
 *  documentation, and that the name of M.I.T. not be used in advertising or
 *  publicity pertaining to distribution of the software without specific,
 *  written prior permission.  M.I.T. makes no representations about the
 *  suitability of this software for any purpose.  It is provided "as is"
 *  without express or implied warranty.
 *
 */

#ifndef _GR4_PACKET_MODEM_RANDOM
#define _GR4_PACKET_MODEM_RANDOM

#include <gnuradio-4.0/packet-modem/xoroshiro128p.h>
#include <chrono>
#include <cmath>
#include <complex>
#include <limits>
#include <numbers>
#include <random>

namespace gr::packet_modem {

/*!
 * \brief wrapper for XOROSHIRO128+ PRNG for use in std::distributions
 * Fulfills C++ named requirements for UniformRandomBitGenerator
 * \ingroup math_blk
 */
class xoroshiro128p_prng
{
public:
    using result_type = uint64_t; //! \brief value type is uint64

private:
    result_type state[2];

public:
    /*!
     * \brief minimum value
     */
    static constexpr result_type min() { return std::numeric_limits<result_type>::min(); }
    /*!
     * \brief maximum value
     */
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    /*!
     * \brief constructor. Expects a seed.
     */
    xoroshiro128p_prng(uint64_t init) { seed(init); }


    /*!
     * \brief yield a random value and advance state
     */
    result_type operator()() { return xoroshiro128p_next(state); }

    /*!
     * \brief set new seed
     */
    void seed(uint64_t seed) { xoroshiro128p_seed(state, seed); }
};

/*!
 * \brief pseudo random number generator
 * \ingroup math_blk
 */
class random
{
protected:
    uint64_t d_seed;
    bool d_gauss_stored;
    float d_gauss_value;

    xoroshiro128p_prng d_rng; // mersenne twister as random number generator
    std::uniform_real_distribution<float>
        d_uniform; // choose uniform distribution, default is [0,1)
    std::uniform_int_distribution<int64_t> d_integer_dis;

public:
    random(uint64_t seed = 0, int64_t min_integer = 0, int64_t max_integer = 2)
        : d_rng(seed), d_integer_dis(0, 1)
    {
        d_gauss_stored =
            false; // set gasdev (gauss distributed numbers) on calculation state

        // Setup random number generators
        set_integer_limits(min_integer, max_integer);
    }

    ~random() {}

    /*!
     * \brief Change the seed for the initialized number generator. seed = 0 initializes
     * the random number generator with the system time.
     */
    void reseed(uint64_t seed)
    {
        /*
         * Seed is initialized with time if the given seed is 0. Otherwise the seed is
         * taken directly. Sets the seed for the random number generator.
         */
        d_seed = seed;
        if (d_seed == 0) {
            auto now = std::chrono::system_clock::now().time_since_epoch();
            auto ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
            d_rng.seed(ns);
        } else {
            d_rng.seed(d_seed);
        }
    }

    /*!
     * set minimum and maximum for integer random number generator.
     * Limits are [minimum, maximum)
     * Default: [0, std::numeric_limits< IntType >::max)]
     */
    void set_integer_limits(int64_t minimum, int64_t maximum)
    {
        // boost expects integer limits defined as [minimum, maximum] which is
        // unintuitive. use the expected half open interval behavior! [minimum, maximum)!
        d_integer_dis = std::uniform_int_distribution<int64_t>(minimum, maximum - 1);
    }

    /*!
     * Uniform random integers in the range set by 'set_integer_limits' [min, max).
     */
    int64_t ran_int() { return d_integer_dis(d_rng); }

    /*!
     * \brief Uniform random numbers in the range [0.0, 1.0)
     */
    float ran1() { return d_uniform(d_rng); }

    /*!
     * \brief Normally distributed random numbers (Gaussian distribution with zero mean
     * and variance 1)
     */
    float gasdev()
    {
        /*
         * Returns a normally distributed deviate with zero mean and variance 1.
         * Used is the Marsaglia polar method.
         * Every second call a number is stored because the transformation works only in
         * pairs. Otherwise half calculation is thrown away.
         */
        if (d_gauss_stored) { // just return the stored value if available
            d_gauss_stored = false;
            return d_gauss_value;
        } else { // generate a pair of gaussian distributed numbers
            float x, y, s;
            do {
                x = 2.0f * ran1() - 1.0f;
                y = 2.0f * ran1() - 1.0f;
                s = x * x + y * y;
            } while (s >= 1.0f || s == 0.0f);
            d_gauss_stored = true;
            d_gauss_value = x * sqrtf(-2.0f * logf(s) / s);
            return y * sqrtf(-2.0f * logf(s) / s);
        }
    }

    /*!
     * \brief Laplacian distributed random numbers with zero mean and variance 1
     */
    float laplacian()
    {
        float z = ran1();
        if (z > 0.5f) {
            return -logf(2.0f * (1.0f - z));
        }
        return logf(2.0f * z);
    }

    /*!
     * \brief Rayleigh distributed random numbers (zero mean and variance 1 for the
     * underlying Gaussian distributions)
     */
    float rayleigh() { return sqrtf(-2.0f * logf(ran1())); }

    /*!
     * \brief Exponentially distributed random numbers with values less than or equal
     * to factor replaced with zero. The underlying exponential distribution has
     * mean sqrt(2) and variance 2.
     */
    float impulse(float factor)
    {
        /*
         * Copied from The KC7WW / OH2BNS Channel Simulator
         * FIXME Need to check how good this is at some point
         */
        // 5 => scratchy, 8 => Geiger
        float z = -std::numbers::sqrt2_v<float> * logf(ran1());
        if (fabsf(z) <= factor)
            return 0.0f;
        else
            return z;
    }

    /*!
     * \brief Normally distributed random numbers with zero mean and variance 1 on real
     * and imaginary part. This results in a Rayleigh distribution for the amplitude and
     * an uniform distribution for the phase.
     */
    std::complex<float> rayleigh_complex() { return std::complex(gasdev(), gasdev()); }
};

}; // namespace gr::packet_modem


#endif // _GR4_PACKET_MODEM_RANDOM
