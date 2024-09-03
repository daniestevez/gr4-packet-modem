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
class SymbolFilter : public gr::Block<SymbolFilter<TIn, TOut, TTaps>, gr::Resampling<>>
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
    static constexpr char syncword_amplitude_key[] = "syncword_amplitude";
    static constexpr char syncword_time_est_key[] = "syncword_time_est";

public:
    // taps in polyphase structure
    std::vector<std::vector<TTaps>> _taps;
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in settingsChanged()
    gr::HistoryBuffer<TIn> _history{ 1 };
    size_t _clock_phase = 0;
    size_t _reset_clock_phase = 0;
    size_t _pfb_arm = 0;
    ssize_t _tag_delay0 = 0;
    ssize_t _tag_delay = 0;
    std::vector<gr::Tag> _tags{};
    float _scale = 1.0;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    size_t samples_per_symbol;
    std::vector<TTaps> taps;
    // number of polyphase arms
    size_t num_arms;
    size_t delay;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (samples_per_symbol == 0) {
            throw gr::exception("samples_per_symbol cannot be zero");
        }

        if (num_arms == 0) {
            throw gr::exception("num_arms cannot be zero");
        }

        // set resampling for the scheduler
        //
        // TODO: this is giving problems with tags (because input tags are not
        // aligned to samples_per_symbol blocks)
        //
        // this->output_chunk_size = 1;
        // this->input_chunk_size = samples_per_symbol;

        // Organize the taps in a polyphase structure
        _taps.resize(num_arms);
        for (size_t j = 0; j < num_arms; ++j) {
            _taps[j].clear();
            for (size_t k = j; k < taps.size(); k += num_arms) {
                _taps[j].push_back(taps[k]);
            }
        }

        // create a history of the appropriate size
        const auto arm_size = _taps[0].size();
        const auto capacity = std::bit_ceil(arm_size);
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (ssize_t j = static_cast<ssize_t>(_history.size()) - 1; j >= 0; --j) {
            new_history.push_back(_history[static_cast<size_t>(j)]);
        }
        _history = new_history;

        // _reset_clock_phase = -delay `mod` sps
        // (typically this will be zero)
        _reset_clock_phase =
            (samples_per_symbol - (delay % samples_per_symbol)) % samples_per_symbol;
    }

    void start() { _clock_phase = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "_clock_phase = {}, _scale = {}, _pfb_arm = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _clock_phase,
                     _scale,
                     _pfb_arm);
#endif
        auto out_item = outSpan.begin();
        auto in_item = inSpan.begin();
        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            ssize_t tag_index_adjust = 0;
            if (tag.map.contains(syncword_amplitude_key)) {
#ifdef TRACE
                fmt::println("{} got {} tag", this->name, syncword_amplitude_key);
#endif
                // The first item is the beginning of the modulated
                // syncword. After pushing back delay + 1 items to the history,
                // the first syncword symbol should be output, which means that
                // _clock_phase should be 0 at that point. Between now and that
                // moment, _clock_phase is incremented delay times, so we adjust
                // _clock_phase to account for this (see calculation for
                // _reset_clock_phase above).
                size_t new_clock_phase = _reset_clock_phase;
                _scale = 1.0f / pmtv::cast<float>(tag.map[syncword_amplitude_key]);

                // time_est is in the range [-0.5, 0.5]
                float time_est = pmtv::cast<float>(tag.map[syncword_time_est_key]);
                // The PFB can only go "forward" in time, so to go back we add 1
                // to _clock_phase
                if (time_est < 0.0f) {
                    new_clock_phase = (new_clock_phase + 1UZ) % samples_per_symbol;
                    time_est += 1.0f;
                    // adjust phase by one sample
                    float syncword_phase = pmtv::cast<float>(tag.map["syncword_phase"]);
                    double syncword_freq = pmtv::cast<double>(tag.map["syncword_freq"]);
                    tag.map["syncword_phase"] = pmtv::pmt(static_cast<float>(
                        static_cast<double>(syncword_phase) - syncword_freq));
                }

                // Special case to avoid skipping one output symbol when
                // _clock_phase == 0 and new_clock_phase == 1
                if (_clock_phase == 0 && new_clock_phase == 1) {
                    _history.push_back(*in_item++);
                    *out_item = _scale * std::inner_product(_taps[_pfb_arm].cbegin(),
                                                            _taps[_pfb_arm].cend(),
                                                            _history.cbegin(),
                                                            TOut{ 0 });
                    // Check to see if we need to output a tag. Tags go on the
                    // "nearest" possible output sample, which means when their
                    // index is in the interval [sps/2, sps/2).
                    while (!_tags.empty() &&
                           _tags[0].index <
                               static_cast<ssize_t>(samples_per_symbol / 2)) {
#ifdef TRACE
                        fmt::println("{} publishTag() {} at index = {}",
                                     this->name,
                                     _tags[0].map,
                                     out_item - outSpan.begin());
#endif
                        out.publishTag(_tags[0].map, out_item - outSpan.begin());
                        _tags.erase(_tags.begin());
                    }
                    ++out_item;
                    ++new_clock_phase;
                    for (auto& t : _tags) {
                        --t.index;
                    }
                    // account for the fact that the index of the current has
                    // not been decreased in this loop
                    tag_index_adjust = -1;
                }
                // Special case to avoid duplicating one output symbol when
                // _clock_phase == 1 and new_clock_phase == 0
                else if (_clock_phase == 1 && new_clock_phase == 0) {
                    _history.push_back(*in_item++);
                    ++new_clock_phase;
                }

                _clock_phase = new_clock_phase;
                // time_est is now in the range [0.0, 1.0]
                _pfb_arm = std::clamp(static_cast<size_t>(std::round(
                                          static_cast<float>(num_arms) * time_est)),
                                      0UZ,
                                      num_arms - 1UZ);
            }
            tag.index = static_cast<ssize_t>(delay) + tag_index_adjust;
            _tags.push_back(tag);
        }

        while (out_item < outSpan.end() && in_item < inSpan.end()) {
            _history.push_back(*in_item++);
            if (_clock_phase == 0) {
                *out_item = _scale * std::inner_product(_taps[_pfb_arm].cbegin(),
                                                        _taps[_pfb_arm].cend(),
                                                        _history.cbegin(),
                                                        TOut{ 0 });
                // Check to see if we need to output a tag. Tags go on the
                // "nearest" possible output sample, which means when their
                // index is in the interval [sps/2, sps/2).
                while (!_tags.empty() &&
                       _tags[0].index < static_cast<ssize_t>(samples_per_symbol / 2)) {
#ifdef TRACE
                    fmt::println("{} publishTag() {} at index = {}",
                                 this->name,
                                 _tags[0].map,
                                 out_item - outSpan.begin());
#endif
                    out.publishTag(_tags[0].map, out_item - outSpan.begin());
                    _tags.erase(_tags.begin());
                }
                ++out_item;
            }
            ++_clock_phase;
            if (_clock_phase >= samples_per_symbol) {
                _clock_phase = 0;
            }
            for (auto& tag : _tags) {
                --tag.index;
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
    gr::packet_modem::SymbolFilter, in, out, samples_per_symbol, taps, num_arms, delay);

#endif // _GR4_PACKET_MODEM_SYMBOL_FILTER
