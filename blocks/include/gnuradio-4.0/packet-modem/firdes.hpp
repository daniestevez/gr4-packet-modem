/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2002,2007,2008,2012,2013,2018 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */


#ifndef _GR4_PACKET_MODEM_INTERPOLATING_FIRDES
#define _GR4_PACKET_MODEM_INTERPOLATING_FIRDES

#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>
#include <vector>

namespace gr::packet_modem::firdes {

// Calculates the taps for a root raised cosine filter.
//
// This function has the same parameters as the GR3
// `gr::filter::firdes::root_raised_cosine()` function and the results are numerically
// equivalent.
template <typename T = float>
constexpr std::vector<T> root_raised_cosine(
    double gain, double sampling_freq, double symbol_rate, double alpha, size_t ntaps)
{
    ntaps |= 1; // ensure that ntaps is odd

    const double spb = sampling_freq / symbol_rate; // samples per bit/symbol
    std::vector<double> taps(ntaps);
    std::ranges::transform(std::views::iota(0UZ, ntaps), taps.begin(), [&](size_t i) {
        const double xindx = static_cast<double>(static_cast<ssize_t>(i) -
                                                 static_cast<ssize_t>(ntaps) / 2);
        const double x1 = std::numbers::pi * xindx / spb;
        double x2 = 4.0 * alpha * xindx / spb;
        double x3 = x2 * x2 - 1.0;

        double num, den;
        if (std::abs(x3) >= 0.000001) { // Avoid Rounding errors...
            if (i != ntaps / 2) {
                num = std::cos((1.0 + alpha) * x1) +
                      std::sin((1.0 - alpha) * x1) / (4.0 * alpha * xindx / spb);
            } else {
                num = std::cos((1.0 + alpha) * x1) +
                      (1.0 - alpha) * std::numbers::pi / (4.0 * alpha);
            }
            den = x3 * std::numbers::pi;
        } else {
            if (alpha == 1.0) {
                return -1.0;
            }
            x3 = (1.0 - alpha) * x1;
            x2 = (1.0 + alpha) * x1;
            num = (std::sin(x2) * (1.0 + alpha) * std::numbers::pi -
                   std::cos(x3) * ((1.0 - alpha) * std::numbers::pi * spb) /
                       (4.0 * alpha * xindx) +
                   std::sin(x3) * spb * spb / (4.0 * alpha * xindx * xindx));
            den = -32.0 * std::numbers::pi * alpha * alpha * xindx / spb;
        }
        return 4.0 * alpha * num / den;
    });

    const double scale = std::accumulate(taps.cbegin(), taps.cend(), 0.0);

    std::vector<T> taps_T(ntaps);
    std::ranges::transform(taps, taps_T.begin(), [&](double tap) {
        return static_cast<T>(tap * gain / scale);
    });
    return taps_T;
}

} // namespace gr::packet_modem::firdes

#endif // _GR4_PACKET_MODEM_INTERPOLATING_FIRDES
