#ifndef _GR4_PACKET_MODEM_PACKET_TRANSMITTER_RRC_TAPS
#define _GR4_PACKET_MODEM_PACKET_TRANSMITTER_RRC_TAPS

#include <gnuradio-4.0/packet-modem/firdes.hpp>

namespace gr::packet_modem {

inline constexpr std::vector<float> packet_transmitter_rrc_taps(size_t samples_per_symbol)
{
    const size_t ntaps = samples_per_symbol * 11U;
    auto rrc_taps = firdes::root_raised_cosine(
        1.0, static_cast<double>(samples_per_symbol), 1.0, 0.35, ntaps);
    // scale rrc_taps for maximum power using [-1, 1] DAC range
    float rrc_taps_sum_abs_max = 0.0f;
    for (size_t j = 0; j < samples_per_symbol; ++j) {
        float rrc_taps_sum_abs = 0.0f;
        for (size_t k = j; k < rrc_taps.size(); k += samples_per_symbol) {
            rrc_taps_sum_abs += std::abs(rrc_taps[k]);
        }
        rrc_taps_sum_abs_max = std::max(rrc_taps_sum_abs_max, rrc_taps_sum_abs);
    }
    // used to avoid reaching DAC full scale
    const float scale = 0.9f;
    for (auto& x : rrc_taps) {
        x *= scale / rrc_taps_sum_abs_max;
    }
    return rrc_taps;
}

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PACKET_TRANSMITTER_RRC_TAPS
