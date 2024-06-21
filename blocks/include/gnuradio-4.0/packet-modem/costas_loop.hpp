#ifndef _GR4_PACKET_MODEM_COSTAS_LOOP
#define _GR4_PACKET_MODEM_COSTAS_LOOP

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/constellation.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <magic_enum.hpp>
#include <cmath>
#include <complex>
#include <numbers>

namespace gr::packet_modem {

template <typename T = float, typename TPhase = float>
class CostasLoop : public gr::Block<CostasLoop<T, TPhase>>
{
public:
    using Description = Doc<R""(
@brief Costas Loop.

)"">;

public:
    TPhase _phase = 0;
    TPhase _freq = 0;
    // Loop coefficients K_1 and K_2
    T _k1 = 0;
    T _k2 = 0;

private:
    static constexpr char syncword_phase_key[] = "syncword_phase";
    static constexpr char constellation_key[] = "constellation";

    void set_phase(T phase)
    {
#ifdef TRACE
        fmt::println("{}::set_phase({})", this->name, phase);
#endif
        _phase = phase;
        _freq = 0;
    }

public:
    gr::PortIn<std::complex<T>> in;
    gr::PortOut<std::complex<T>> out;
    // B_L * T loop bandwidth parameter
    double loop_bandwidth = 0.01;
    Constellation _constellation = Constellation::BPSK;
    std::string constellation{ magic_enum::enum_name(_constellation) };

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
#ifdef TRACE
        fmt::println(
            "{}::settingsChanged() constellation = {}", this->name, constellation);
#endif
        _constellation = magic_enum::enum_cast<Constellation>(
                             constellation, magic_enum::case_insensitive)
                             .value();
        double discriminant_gain = 1.0;
        if (_constellation == Constellation::QPSK) {
            discriminant_gain = std::numbers::sqrt2;
        }

        // Solve cubic equation in terms of loop_bandwidth to get K_1 and K_2
        const double loop_bandwidth_2 = loop_bandwidth * loop_bandwidth;
        const double loop_bandwidth_3 = loop_bandwidth_2 * loop_bandwidth;
        const double loop_bandwidth_4 = loop_bandwidth_2 * loop_bandwidth_2;
        const double s = std::cbrt(
            36.0 * loop_bandwidth_2 +
            std::sqrt(3.0) *
                std::sqrt(432.0 * loop_bandwidth_4 + 848.0 * loop_bandwidth_3 +
                          624.0 * loop_bandwidth_2 + 204.0 * loop_bandwidth + 25.0) +
            36.0 * loop_bandwidth + 9.0);
        const double z =
            -(-12.0 * loop_bandwidth - 6.0) /
                (3.0 * std::cbrt(6.0) * (2.0 * loop_bandwidth + 1.0) * s) +
            (std::cbrt(2.0) * s) / (std::cbrt(9.0) * (2.0 * loop_bandwidth + 1.0)) - 1.0;
        const double k1 = 1.0 - z * z;
        const double k2 = (1.0 - z) * (1.0 - z);
#ifdef TRACE
        fmt::println("{} k1 = {}, k2 = {}", this->name, k1, k2);
#endif
        _k1 = static_cast<T>(k1 / discriminant_gain);
        _k2 = static_cast<T>(k2 / discriminant_gain);
    }

    // processBulk() used instead of processOne() for the same reason as in
    // AdditiveScrambler
    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        if (this->input_tags_present()) {
            const auto tag = this->mergedInputTag();
            if (tag.map.contains(syncword_phase_key)) {
                set_phase(pmtv::cast<TPhase>(tag.map.at(syncword_phase_key)));
            }
        }
#ifdef TRACE
        fmt::println("{}::processBulk() constellation = {}, _phase = {}",
                     this->name,
                     constellation,
                     _phase);
#endif
        for (size_t j = 0; j < inSpan.size(); ++j) {
            const std::complex<T> lo = { std::cos(static_cast<T>(_phase)),
                                         -std::sin(static_cast<T>(_phase)) };
            const std::complex<T> z_out = inSpan[j] * lo;
            outSpan[j] = z_out;
            T error = 0;
            switch (_constellation) {
            case Constellation::PILOT:
                // phase discriminant for pure pilot is Q
                error = z_out.imag();
                break;
            case Constellation::BPSK:
                // phase discriminant for BPSK is I*Q
                error = z_out.real() * z_out.imag();
                break;
            case Constellation::QPSK:
                // Phase discriminant for QPSK is (Q - I))/sqrt(2) assuming the
                // signal is in the first quadrant. The /sqrt(2) term is taken
                // into account in the calculation of _k1 and _k2.
                error = (z_out.real() > 0 ? z_out.imag() : -z_out.imag()) +
                        (z_out.imag() > 0 ? -z_out.real() : z_out.real());
                break;
            default:
                // should not be reached
                abort();
            }
            _freq += static_cast<TPhase>(_k2 * error);
            _phase += static_cast<TPhase>(_k1 * error) + _freq;
            if (_phase >= std::numbers::pi_v<TPhase>) {
                _phase -= TPhase{ 2 } * std::numbers::pi_v<TPhase>;
            } else if (_phase < -std::numbers::pi_v<TPhase>) {
                _phase += TPhase{ 2 } * std::numbers::pi_v<TPhase>;
            }
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::CostasLoop, in, out, loop_bandwidth, constellation);

#endif // _GR4_PACKET_MODEM_COSTAS_LOOP
