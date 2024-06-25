#ifndef _GR4_PACKET_MODEM_SYNCWORD_DETECTION
#define _GR4_PACKET_MODEM_SYNCWORD_DETECTION

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/HistoryBuffer.hpp>
#include <gnuradio-4.0/algorithm/fourier/fftw.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <complex>
#include <numbers>
#include <numeric>
#include <ranges>

namespace gr::packet_modem {

namespace syncword_detection {
struct HistoryItem {
    using c64 = std::complex<float>;
    c64 sample;
    float correlation_power;
    c64 correlation;
    int freq_bin;
    bool detection = false;
};
} // namespace syncword_detection

class SyncwordDetection : public gr::Block<SyncwordDetection>
{
public:
    using Description = Doc<R""(
@brief Syncword Detection.

This block performs syncword detection by doing a correlation against the
expected modulated syncword using a long FFT with the overlap-save
method. Multiple frequency offsets spaced by 0.5 divided by the syncword
duration can be tried (this is controlled using the `min_freq_bin` and
`max_freq_bin` parameters).

A successful detection is declared whenever a correlation attains the maximum in
a time window of +/-`time_threshold` and such maximum is greater than
`power_threshold` times the median in that window. On detection, a tag is
attached to the item where the modulated syncword begins. The tag indicates
carrier phase and frequency of the syncword.

)"">;

private:
    using c64 = std::complex<float>;
    using FFT = gr::algorithm::FFTw<c64, c64>;

    gr::property_map output_tag(const syncword_detection::HistoryItem& item) const
    {
        // divide by fft_size to account for the fact that the IFFT is missing a
        // 1/N factor
        const float syncword_amplitude =
            std::sqrt(item.correlation_power) /
            (static_cast<float>(fft_size) * _syncword_self_corr);
        const double syncword_freq = static_cast<double>(item.freq_bin) *
                                     std::numbers::pi /
                                     static_cast<double>(_syncword_samples_size);
        return {
            { "syncword_amplitude", syncword_amplitude },
            { "syncword_phase", std::arg(item.correlation) },
            { "syncword_freq", syncword_freq },
            { "syncword_freq_bin", item.freq_bin },
        };
    }

public:
    size_t _syncword_samples_size;
    FFT _fft;
    // one vector for each freq bin
    std::vector<std::vector<c64>> _syncword_fft_conj;
    float _syncword_self_corr;
    float _best;
    uint64_t _best_idx;
    uint64_t _items_consumed;
    size_t _history_size;
    // placeholder size; assigned in start()
    HistoryBuffer<syncword_detection::HistoryItem> _history{ 2 };

public:
    gr::PortIn<std::complex<float>> in;
    gr::PortOut<std::complex<float>> out;
    size_t fft_size = 2048;
    size_t samples_per_symbol = 4;
    std::vector<float> rrc_taps;
    std::vector<uint8_t> syncword;
    std::vector<std::complex<float>> constellation;
    int min_freq_bin = 0;
    int max_freq_bin = 0;
    uint64_t time_threshold = 768;
    float power_threshold = 10.0;

    void start()
    {
        if (min_freq_bin > max_freq_bin) {
            throw gr::exception("min_freq_bin is greater than max_freq_bin");
        }
        _syncword_samples_size =
            (syncword.size() - 1) * samples_per_symbol + rrc_taps.size();
        if (_syncword_samples_size > fft_size) {
            throw gr::exception("fft_size too small");
        }

        std::vector<c64> syncword_samples(_syncword_samples_size);
        for (size_t j = 0; j < syncword.size(); ++j) {
            for (size_t k = 0; k < rrc_taps.size(); ++k) {
                syncword_samples[j * samples_per_symbol + k] +=
                    constellation[syncword[j]] * rrc_taps[k];
            }
        }
        _syncword_self_corr = 0.0f;
        for (auto x : syncword_samples) {
            _syncword_self_corr += x.real() * x.real() + x.imag() * x.imag();
        }

        for (int freq_bin = min_freq_bin; freq_bin <= max_freq_bin; ++freq_bin) {
            // shift syncword samples in frequency; each frequency bin is 1/2 of
            // the coherent integration interval
            double phase = 0.0;
            const double phase_incr = static_cast<double>(freq_bin) * std::numbers::pi /
                                      static_cast<double>(_syncword_samples_size);
            std::vector<c64> syncword_samples_shifted = syncword_samples;
            for (auto& x : syncword_samples_shifted) {
                x *= std::complex<float>{ static_cast<float>(std::cos(phase)),
                                          static_cast<float>(std::sin(phase)) };
                phase += phase_incr;
                if (phase >= std::numbers::pi) {
                    phase -= 2.0 * std::numbers::pi;
                } else if (phase < std::numbers::pi) {
                    phase += 2.0 * std::numbers::pi;
                }
            }
            syncword_samples_shifted.resize(fft_size);
            auto syncword_fft_conj = _fft.compute(syncword_samples_shifted);
            for (auto& z : syncword_fft_conj) {
                z = std::conj(z);
            }
            _syncword_fft_conj.push_back(syncword_fft_conj);
        }

        _best = 0.0f;
        _best_idx = 0;
        _items_consumed = 0;
        _history_size = 2 * time_threshold + 1;
        _history =
            HistoryBuffer<syncword_detection::HistoryItem>(std::bit_ceil(_history_size));
        in.min_samples = fft_size;
        out.min_samples = fft_size;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        // it seems that the scheduler isn't respecting the rules of min_samples when
        // calling this block
        if (inSpan.size() < fft_size) {
            if (!inSpan.consume(0)) {
                throw gr::exception("consume failed");
            }
            outSpan.publish(0);
#ifdef TRACE
            fmt::println("{} called with {} input items, but minimum is {}",
                         this->name,
                         inSpan.size(),
                         in.min_samples);
#endif
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        assert(inSpan.size() == outSpan.size());
        assert(inSpan.size() >= fft_size);
        const size_t num_freq_bins = static_cast<size_t>(max_freq_bin - min_freq_bin + 1);
        std::vector<c64> samples_fft;
        std::vector<c64> samples_fft_prod(fft_size);
        std::vector<std::vector<c64>> correlation(num_freq_bins);
        const size_t stride = fft_size - _syncword_samples_size + 1;
        size_t j;
        for (j = 0; j + fft_size <= inSpan.size(); j += stride) {
            samples_fft =
                _fft.compute(inSpan | std::views::drop(j) | std::views::take(fft_size),
                             std::move(samples_fft));
            // Compute IFFT(FFT(samples) * conj(FFT(syncword))
            //
            // The IFFT is computed as an FFT, so the sign of its time axis is
            // flipped.
            for (size_t nfreq = 0; nfreq < num_freq_bins; ++nfreq) {
                for (size_t k = 0; k < fft_size; ++k) {
                    samples_fft_prod[k] = samples_fft[k] * _syncword_fft_conj[nfreq][k];
                }
                correlation[nfreq] =
                    _fft.compute(samples_fft_prod, std::move(correlation[nfreq]));
            }

            for (size_t k = 0; k < stride; ++k) {
                const uint64_t curr_idx = _items_consumed + j + k;
                if (curr_idx - _best_idx > time_threshold) {
                    // check if the value _best / power_threshold is above the
                    // history median by counting if more than half of the
                    // history items are below this value
                    size_t below_threshold = 0;
                    for (size_t u = 0; u < _history_size; ++u) {
                        if (_history[u].correlation_power < _best / power_threshold) {
                            ++below_threshold;
                        }
                    }
                    if (2 * below_threshold >= _history_size) {
#ifdef TRACE
                        fmt::println(
                            "{} best found: _best = {}, _best_idx = {}, curr_idx = {}",
                            this->name,
                            _best,
                            _best_idx,
                            curr_idx);
#endif
                        // the items in the history have these indices:
                        // _curr_idx - _history_size, _curr_idx - _history_size + 1, ...,
                        // _curr_idx - 1.
                        const size_t hist_idx = _best_idx + _history_size - curr_idx;
                        // the history indices are time-reversed: _history[0] is the
                        // item most recently pushed into the history
                        _history[_history_size - 1 - hist_idx].detection = true;
                    }
                    _best = 0.0f;
                    _best_idx = curr_idx;
                }
                // This takes into account the time-reversal of the correlation
                const size_t z_idx = k == 0 ? 0 : fft_size - k;
                size_t best_freq = 0;
                c64 z;
                float zpow = -1.0;
                // Find best frequency bin
                for (size_t nfreq = 0; nfreq < num_freq_bins; ++nfreq) {
                    const c64 zz = correlation[nfreq][z_idx];
                    const float zzpow = zz.real() * zz.real() + zz.imag() * zz.imag();
                    if (zzpow > zpow) {
                        best_freq = nfreq;
                        z = zz;
                        zpow = zzpow;
                    }
                }
                if (zpow > _best) {
                    _best = zpow;
                    _best_idx = curr_idx;
                }
                const auto& pop_history = _history[_history_size - 1];
                outSpan[j + k] = pop_history.sample;
                if (pop_history.detection) {
                    out.publishTag(output_tag(pop_history), static_cast<ssize_t>(j + k));
                }
                syncword_detection::HistoryItem item;
                item.sample = inSpan[j + k];
                item.correlation_power = zpow;
                item.correlation = z;
                item.freq_bin = min_freq_bin + static_cast<int>(best_freq);
                _history.push_back(item);
            }
        }

        if (!inSpan.consume(j)) {
            throw gr::exception("consume failed");
        }
        _items_consumed += j;
        outSpan.publish(j);
#ifdef TRACE
        fmt::println("{} consume & publish = {}", this->name, j);
#endif

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

// constellation not included because std::complex cannot be passed using settings
ENABLE_REFLECTION(gr::packet_modem::SyncwordDetection,
                  in,
                  out,
                  fft_size,
                  samples_per_symbol,
                  rrc_taps,
                  syncword,
                  min_freq_bin,
                  max_freq_bin,
                  time_threshold,
                  power_threshold);

#endif // _GR4_PACKET_MODEM_SYNCWORD_DETECTION
