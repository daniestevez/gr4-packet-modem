#ifndef _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER
#define _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/HistoryBuffer.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <numeric>
#include <ranges>
#include <vector>

namespace gr::packet_modem {

template <typename TIn, typename TOut = TIn, typename TTaps = TIn>
class InterpolatingFirFilter
    : public gr::Block<InterpolatingFirFilter<TIn, TOut, TTaps>, gr::Resampling<>>
{
public:
    using Description = Doc<R""(
@brief Interpolating FIR filter.

This blocks implements an interpolating FIR filter. The interpolation factor is
defined by the `interpolation` parameter. The filter taps are given in the
`taps` parameter. The block is a template and has arguments `TIn`, `TOut`, and
`TTaps` to define the types of the input items, output items and taps respectively.

)"">;

public:
    // taps in a polyphase structure
    std::vector<std::vector<TTaps>> _taps_polyphase;
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in settingsChanged()
    gr::HistoryBuffer<TIn> _history{ 1 };

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    size_t interpolation;
    std::vector<TTaps> taps;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (interpolation == 0) {
            throw gr::exception("interpolation cannot be zero");
        }

        // set resampling for the scheduler
        this->input_chunk_size = 1;
        this->output_chunk_size = interpolation;

        // organize the taps in a polyphase structure
        _taps_polyphase.resize(interpolation);
        for (size_t j = 0; j < interpolation; ++j) {
            _taps_polyphase[j].clear();
            for (size_t k = j; k < taps.size(); k += interpolation) {
                _taps_polyphase[j].push_back(taps[k]);
            }
        }

        // create a history of the appropriate size
        const auto capacity =
            std::bit_ceil((taps.size() + interpolation - 1) / interpolation);
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (const auto j :
             std::views::iota(0UZ, _history.size()) | std::views::reverse) {
            new_history.push_back(_history[j]);
        }
        _history = new_history;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
        if (this->input_tags_present()) {
            const auto tag = this->mergedInputTag();
            fmt::println("{} tags present: map = {}", this->name, tag.map);
        } else {
            fmt::println("{} no tags present", this->name);
        }
#endif
        assert(inSpan.size() == outSpan.size() / interpolation);
        auto out_item = outSpan.begin();
        for (const auto& in_item : inSpan) {
            _history.push_back(in_item);
            for (const auto& branch : _taps_polyphase) {
                *out_item++ = std::inner_product(
                    branch.cbegin(), branch.cend(), _history.cbegin(), TOut{ 0 });
            }
        }

        return gr::work::Status::OK;
    }
};

template <typename TIn, typename TOut, typename TTaps>
class InterpolatingFirFilter<Pdu<TIn>, Pdu<TOut>, TTaps>
    : public gr::Block<InterpolatingFirFilter<Pdu<TIn>, Pdu<TOut>, TTaps>>
{
public:
    using Description = InterpolatingFirFilter<TIn, TOut, TTaps>::Description;

public:
    // taps in a polyphase structure
    std::vector<std::vector<TTaps>> _taps_polyphase;
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in settingsChanged()
    gr::HistoryBuffer<TIn> _history{ 1 };

public:
    gr::PortIn<Pdu<TIn>> in;
    gr::PortOut<Pdu<TOut>> out;
    size_t interpolation;
    std::vector<TTaps> taps;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (interpolation == 0) {
            throw gr::exception("interpolation cannot be zero");
        }

        // organize the taps in a polyphase structure
        _taps_polyphase.resize(interpolation);
        for (size_t j = 0; j < interpolation; ++j) {
            _taps_polyphase[j].clear();
            for (size_t k = j; k < taps.size(); k += interpolation) {
                _taps_polyphase[j].push_back(taps[k]);
            }
        }

        // create a history of the appropriate size
        const auto capacity =
            std::bit_ceil((taps.size() + interpolation - 1) / interpolation);
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (const auto j :
             std::views::iota(0UZ, _history.size()) | std::views::reverse) {
            new_history.push_back(_history[j]);
        }
        _history = new_history;
    }

    [[nodiscard]] Pdu<TOut> processOne(const Pdu<TIn>& pdu)
    {
        Pdu<TOut> pdu_out;

        pdu_out.data.reserve(pdu.data.size() * interpolation);
        for (const auto& in_item : pdu.data) {
            _history.push_back(in_item);
            for (const auto& branch : _taps_polyphase) {
                pdu_out.data.push_back(std::inner_product(
                    branch.cbegin(), branch.cend(), _history.cbegin(), TOut{ 0 }));
            }
        }

        pdu_out.tags.resize(pdu.tags.size());
        std::ranges::transform(pdu.tags, pdu_out.tags.begin(), [&](gr::Tag tag) {
            tag.index *= static_cast<decltype(tag.index)>(interpolation);
            return tag;
        });

        return pdu_out;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::InterpolatingFirFilter, in, out, interpolation, taps);

#endif // _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER
