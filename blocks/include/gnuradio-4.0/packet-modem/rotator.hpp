/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2003,2008,2013,2014 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_ROTATOR
#define _GR4_PACKET_MODEM_ROTATOR

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>

namespace gr::packet_modem {

template <typename T = float>
class Rotator : public gr::Block<Rotator<T>>
{
public:
    using Description = Doc<R""(
@brief Rotator.

This block shifts the frequency of the input signal by rotating it which a
complex exponential whose phase increases by the ``phase_incr`` parameter with
each sample.

)"">;

public:
    std::complex<T> _exp = { T{ 1 }, T{ 0 } };
    std::complex<T> _exp_incr = { T{ 1 }, T{ 0 } };
    unsigned _counter = 0;

public:
    gr::PortIn<std::complex<T>> in;
    gr::PortOut<std::complex<T>> out;
    // phase increment in rad / sample
    T phase_incr = 0;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        _exp_incr = { std::cos(phase_incr), std::sin(phase_incr) };
    }

    void start()
    {
        _exp = { T{ 1 }, T{ 0 } };
        _counter = 0;
    }

    [[nodiscard]] constexpr std::complex<T> processOne(std::complex<T> a) noexcept
    {
        const std::complex<T> z = a * _exp;
        _exp *= _exp_incr;
        if ((++_counter % 512) == 0) {
            // normalize to unit amplitude
            _exp /= std::abs(_exp);
        }
        return z;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Rotator, in, out, phase_incr);

#endif // _GR4_PACKET_MODEM_ROTATOR
