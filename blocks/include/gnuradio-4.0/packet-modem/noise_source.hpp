/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2004,2010,2012,2018 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_NOISE_SOURCE
#define _GR4_PACKET_MODEM_NOISE_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/random.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <magic_enum.hpp>
#include <type_traits>
#include <complex>

namespace gr::packet_modem {

enum class NoiseType { UNIFORM, GAUSSIAN, LAPLACIAN, IMPULSE };

template <typename T>
class NoiseSource : public gr::Block<NoiseSource<T>>
{
public:
    using Description = Doc<R""(
@brief Noise Source.

Outputs noise generated according to a distribution indicated as the template
parameter `noise_type`.

)"">;

public:
    random _rng;

public:
    gr::PortOut<T> out;
    NoiseType _noise_type = NoiseType::UNIFORM;
    std::string noise_type = std::string(magic_enum::enum_name(_noise_type));
    float amplitude = 1.0;
    float _amplitude_complex = amplitude / std::numbers::sqrt2_v<float>;
    uint64_t seed = 0;

    void settingsChanged(const property_map& /*oldSettings*/,
                         const property_map& /*newSettings*/)
    {
        _noise_type =
            magic_enum::enum_cast<NoiseType>(noise_type, magic_enum::case_insensitive)
                .value();
        _amplitude_complex = amplitude / std::numbers::sqrt2_v<float>;
    }

    void start() { _rng = random(seed); }

    gr::work::Status processBulk(PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, outSpan.size());
#endif
        if constexpr (std::is_same_v<T, std::complex<float>> ||
                      std::is_same_v<T, std::complex<double>>) {
            switch (_noise_type) {
            case NoiseType::UNIFORM:
                for (auto& x : outSpan) {
                    x = std::complex<float>(
                        _amplitude_complex * ((_rng.ran1() * 2.0f) - 1.0f),
                        _amplitude_complex * ((_rng.ran1() * 2.0f) - 1.0f));
                }
                break;
            case NoiseType::GAUSSIAN:
                for (auto& x : outSpan) {
                    x = _amplitude_complex * _rng.rayleigh_complex();
                }
                break;
            default:
                throw gr::exception("invalid noise_type");
            }
        } else {
            switch (_noise_type) {
            case NoiseType::UNIFORM:
                for (auto& x : outSpan) {
                    x = static_cast<T>(amplitude * ((_rng.ran1() * 2.0f) - 1.0f));
                }
                break;
            case NoiseType::GAUSSIAN:
                for (auto& x : outSpan) {
                    x = static_cast<T>(amplitude * _rng.gasdev());
                }
                break;
            case NoiseType::LAPLACIAN:
                for (auto& x : outSpan) {
                    x = static_cast<T>(amplitude * _rng.laplacian());
                }
                break;
            case NoiseType::IMPULSE:
                for (auto& x : outSpan) {
                    // GNU Radio 3.10 has the following note:
                    // FIXME changeable impulse settings
                    x = static_cast<T>(amplitude * _rng.impulse(9));
                }
                break;
            default:
                throw gr::exception("invalid noise_type");
            }
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::NoiseSource, out, noise_type, amplitude, seed);

#endif // _GR4_PACKET_MODEM_NOISE_SOURCE
