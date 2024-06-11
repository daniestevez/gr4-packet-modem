#ifndef _GR4_PACKET_MODEM_SYMBOL_FILTER
#define _GR4_PACKET_MODEM_SYMBOL_FILTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/HistoryBuffer.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <numeric>
#include <vector>

namespace gr::packet_modem {

template <typename TIn, typename TOut = TIn, typename TTaps = TIn>
class SymbolFilter
    : public gr::Block<SymbolFilter<TIn, TOut, TTaps>, gr::ResamplingRatio<>>
{
public:
    using Description = Doc<R""(
@brief Symbol filter.

This block implements matched pulse shape filtering and decimation to one sample
per symbol. The block expects to receive tags from the SyncwordDetection block
in order to align itself to the correct phase of the symbol clock. If it does
not receive tags, it keeps free-running with the same symbol clock phase.

The block also normalizes the signal amplitude by using the syncword_amplitude
tags produced by the SyncwordDetection block.

The filter taps are given in the `taps` parameter. The block is a template and
has arguments `TIn`, `TOut`, and `TTaps` to define the types of the input items,
output items and taps respectively.

)"">;

private:
    static constexpr std::string syncword_amplitude_key = "syncword_amplitude";

public:
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in settingsChanged()
    gr::HistoryBuffer<TIn> _history{ 1 };
    size_t _clock_phase = 0;
    size_t _reset_clock_phase = 0;
    ssize_t _tag_delay = 0;
    std::vector<gr::Tag> _tags{};
    float _scale = 1.0;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    size_t samples_per_symbol;
    std::vector<TTaps> taps;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (samples_per_symbol == 0) {
            throw gr::exception("samples_per_symbol cannot be zero");
        }

        // set resampling ratio for the scheduler
        //
        // TODO: this is giving problems with tags (because input tags are not
        // aligned to samples_per_symbol blocks)
        //
        // this->numerator = 1;
        // this->denominator = samples_per_symbol;

        // create a history of the appropriate size
        const auto capacity = std::bit_ceil(taps.size());
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (ssize_t j = static_cast<ssize_t>(_history.size()) - 1; j >= 0; --j) {
            new_history.push_back(_history[static_cast<size_t>(j)]);
        }
        _history = new_history;

        const int ntaps = static_cast<int>(taps.size());
        const int sps = static_cast<int>(samples_per_symbol);
        // _reset_clock_phase = -(ntaps - 1) `mod` sps
        _reset_clock_phase = static_cast<size_t>((((1 - ntaps) % sps) + sps) % sps);
        // when the clock phase is reset, after pushing back taps.size() taps,
        // incrementing the clock_phase by taps.size() - 1, the following
        // outputs have been produced
        _tag_delay = static_cast<ssize_t>(
            (ntaps - 1 + static_cast<int>(_reset_clock_phase)) / sps);
    }

    void start() { _clock_phase = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "_clock_phase = {}, _scale = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _clock_phase,
                     _scale);
#endif
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            if (tag.map.contains(syncword_amplitude_key)) {
#ifdef TRACE
                fmt::println("{} got {} tag", this->name, syncword_amplitude_key);
#endif
                // The first item is the beginning of the modulated syncword. After
                // pushing back taps.size() items to the history, the first syncword
                // symbol should be output, which means that _clock_phase should be
                // 0 at that point. Between now and that moment, _clock_phase is
                // incremented taps.size() - 1 times, so we adjust _clock_phase to
                // account for this (see calculation for _reset_clock_phase above).
                _clock_phase = _reset_clock_phase;
                _scale = 1.0f / pmtv::cast<float>(tag.map[syncword_amplitude_key]);
            }
            tag.index = _tag_delay;
            _tags.push_back(tag);
        }
        auto out_item = outSpan.begin();
        auto in_item = inSpan.begin();
        while (out_item < outSpan.end() && in_item < inSpan.end()) {
            _history.push_back(*in_item++);
            if (_clock_phase == 0) {
                *out_item = _scale *
                            std::inner_product(
                                taps.cbegin(), taps.cend(), _history.cbegin(), TOut{ 0 });
                // check to see if we need to output a tag, and update tag delays
                if (!_tags.empty()) {
                    if (_tags[0].index == 0) {
                        out.publishTag(_tags[0].map, out_item - outSpan.begin());
                        _tags.erase(_tags.begin());
                    }
                    for (auto& tag : _tags) {
                        assert(tag.index > 0);
                        --tag.index;
                    }
                }
                ++out_item;
            }
            ++_clock_phase;
            if (_clock_phase >= samples_per_symbol) {
                _clock_phase = 0;
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
    gr::packet_modem::SymbolFilter, in, out, samples_per_symbol, taps);

#endif // _GR4_PACKET_MODEM_SYMBOL_FILTER
