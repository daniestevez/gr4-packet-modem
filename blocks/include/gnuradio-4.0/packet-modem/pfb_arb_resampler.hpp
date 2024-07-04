/*
 * This file is loosely based in code from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2014 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */


#ifndef _GR4_PACKET_MODEM_PFB_ARB_RESAMPLER
#define _GR4_PACKET_MODEM_PFB_ARB_RESAMPLER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/HistoryBuffer.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <type_traits>
#include <vector>

namespace gr::packet_modem {

template <typename TIn, typename TOut = TIn, typename TTaps = TIn, typename TRate = float>
class PfbArbResampler : public gr::Block<PfbArbResampler<TIn, TOut, TTaps, TRate>>
{
public:
    using Description = Doc<R""(
@brief Polyphase Arbitrary Resampler.

This block is a polyphase arbitrary resampler. The `rate` parameter indicates
the number of output samples per input sample. The `taps` parameter gives the
taps of the prototype filter. The `filter_size` parameter gives the number of
arms (branches) in the polyphase filter.

The type parameters `TIn` and `TOut` indicate the types of the input and output
items respectively. The type parameter `TTaps` indicates the type of the filter
taps. The type `TRate` is the type of the `rate` variable and of the internal
variables used to control resampling. In many cases, `float` can be used as
`TRate`, but for more precise results (for instance, for resampling ratios very
close to one), `double` can be used.

)"">;

public:
    // taps in a polyphase structure
    std::vector<std::vector<TTaps>> _taps;
    // derivative filter taps in a polyphase structure
    std::vector<std::vector<TTaps>> _diff_taps;
    size_t _arm_size;
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in settingsChanged()
    gr::HistoryBuffer<TIn> _history{ 1 };
    size_t _decim_rate;
    TRate _filt_rate;
    size_t _last_filter;
    TRate _phase_acc;

public:
    // Both input and output ports set to Async because this block doesn't have
    // a resampling ratio that can be expressed as a fraction.
    gr::PortIn<TIn, gr::Async> in;
    gr::PortOut<TOut, gr::Async> out;
    TRate rate{ 1.0 };
    std::vector<TTaps> taps;
    size_t filter_size = 32UZ;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (filter_size == 0) {
            throw gr::exception("filter_size cannot be 0");
        }

        _arm_size = (taps.size() + filter_size - 1) / filter_size;

        // Organize the taps in a polyphase structure
        _taps.resize(filter_size);
        for (size_t j = 0; j < filter_size; ++j) {
            _taps[j].clear();
            for (size_t k = j; k < taps.size(); k += filter_size) {
                _taps[j].push_back(taps[k]);
            }
            while (_taps[j].size() < _arm_size) {
                _taps[j].push_back(TTaps{ 0 });
            }
        }

        //
        // The derivative filter is
        //   y[n] = x[n + 1] - x[n],  for n < x.size() - 1
        //   y[x.size() - 1] = 0
        // Compute the filter on the fly as it is written into a polyphase structure
        _diff_taps.resize(filter_size);
        for (size_t j = 0; j < filter_size; ++j) {
            _diff_taps[j].clear();
            for (size_t k = j; k < taps.size() - 1; k += filter_size) {
                _diff_taps[j].push_back(taps[k + 1] - taps[k]);
            }
            while (_diff_taps[j].size() < _arm_size) {
                _diff_taps[j].push_back(TTaps{ 0 });
            }
        }

        // create a history of the appropriate size
        const auto capacity = std::bit_ceil(_arm_size);
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (ssize_t j = static_cast<ssize_t>(_history.size()) - 1; j >= 0; --j) {
            new_history.push_back(_history[static_cast<size_t>(j)]);
        }
        _history = new_history;

        const TRate float_rate = static_cast<TRate>(filter_size) / rate;
        _decim_rate = static_cast<size_t>(std::floor(float_rate));
        _filt_rate = float_rate - static_cast<TRate>(_decim_rate);
        _phase_acc = TRate{ 0 };
        _last_filter = (taps.size() / 2) % filter_size;
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
        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();

        while (in_item < inSpan.end() && out_item < outSpan.end()) {
            while (_last_filter >= filter_size && in_item < inSpan.end()) {
                _history.push_back(*in_item++);
                _last_filter -= filter_size;
            }
            if (_last_filter >= filter_size) {
                // not enough input items to continue
                break;
            }
            const TOut filt_out = std::inner_product(_taps[_last_filter].cbegin(),
                                                     _taps[_last_filter].cbegin() +
                                                         static_cast<ssize_t>(_arm_size),
                                                     _history.cbegin(),
                                                     TOut{ 0 });
            const TOut diff_out = std::inner_product(_diff_taps[_last_filter].cbegin(),
                                                     _diff_taps[_last_filter].cbegin() +
                                                         static_cast<ssize_t>(_arm_size),
                                                     _history.cbegin(),
                                                     TOut{ 0 });
            TOut diff_out_scaled;
            if constexpr (std::is_floating_point_v<TOut>) {
                diff_out_scaled = static_cast<TOut>(_phase_acc) * diff_out;
            } else {
                // assume that TOut is a std::complex<> type
                diff_out_scaled = static_cast<TOut::value_type>(_phase_acc) * diff_out;
            }
            *out_item++ = filt_out + diff_out_scaled;
            _phase_acc += _filt_rate;
            _last_filter += _decim_rate;
            if (_phase_acc > TRate{ 1 }) {
                _phase_acc -= TRate{ 1 };
                ++_last_filter;
            }
        }

        if (!inSpan.consume(static_cast<size_t>(in_item - inSpan.begin()))) {
            throw gr::exception("consume failed");
        }
        outSpan.publish(static_cast<size_t>(out_item - outSpan.begin()));

#ifdef TRACE
        fmt::println("{} consumed = {}, published = {}",
                     this->name,
                     in_item - inSpan.begin(),
                     out_item - outSpan.begin());
#endif

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::PfbArbResampler, in, out, rate, taps, filter_size);

#endif // _GR4_PACKET_MODEM_PFB_ARB_RESAMPLER
