#ifndef _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER
#define _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/HistoryBuffer.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <numeric>
#include <vector>

namespace gr::packet_modem {

template <typename TIn, typename TOut = TIn, typename TTaps = TIn>
class InterpolatingFirFilter
    : public gr::Block<InterpolatingFirFilter<TIn, TOut, TTaps>, gr::ResamplingRatio<>>
{
public:
    using Description = Doc<R""(
@brief Interpolating FIR filter.

This blocks implements an interpolating FIR filter. The interpolation factor is
defined by the `interpolation` parameter. The filter taps are given in the
`taps` parameter. The block is a template and has arguments `TIn`, `TOut`, and
`TTaps` to define the types of the input items, output items and taps respectively.

)"">;

private:
    const size_t d_interpolation;
    // taps in a polyphase structure
    std::vector<std::vector<TTaps>> d_taps;
    // the history constructed here is a placeholder; an appropriate history is
    // constructed in set_taps()
    gr::HistoryBuffer<TIn> d_history{ 1 };

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    InterpolatingFirFilter(size_t interpolation, const std::vector<TTaps>& taps)
        : d_interpolation(interpolation)
    {
        if (interpolation == 0) {
            throw std::invalid_argument("interpolation cannot be zero");
        }

        // set resampling ratio for the scheduler
        this->numerator = interpolation;
        this->denominator = 1;

        set_taps(taps);
    }

    void set_taps(const std::vector<TTaps>& taps)
    {
        // organize the taps in a polyphase structure
        d_taps.resize(d_interpolation);
        for (size_t j = 0; j < d_interpolation; ++j) {
            d_taps[j].clear();
            for (size_t k = j; k < taps.size(); k += d_interpolation) {
                d_taps[j].push_back(taps[k]);
            }
        }

        // create a history of the appropriate size
        const auto capacity =
            std::bit_ceil((taps.size() + d_interpolation - 1) / d_interpolation);
        auto new_history = gr::HistoryBuffer<TIn>(capacity);
        // fill history with zeros to avoid problems with undefined history contents
        new_history.push_back_bulk(std::views::repeat(TIn{ 0 }, capacity));
        // move old history items to the new history
        for (ssize_t j = static_cast<ssize_t>(d_history.size()) - 1; j >= 0; --j) {
            new_history.push_back(d_history[static_cast<size_t>(j)]);
        }
        d_history = new_history;
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println(
            "InterpolatingFirFilter::processBulk(inSpan.size() = {}, outSpan.size = {})",
            inSpan.size(),
            outSpan.size());
#endif
        const auto to_consume = std::min(inSpan.size(), outSpan.size() / d_interpolation);
        if (to_consume == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return inSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        auto out_item = outSpan.begin();
        for (const auto& in_item : inSpan | std::views::take(to_consume)) {
            d_history.push_back(in_item);
            for (const auto& branch : d_taps) {
                *out_item++ = std::inner_product(
                    branch.cbegin(), branch.cend(), d_history.cbegin(), TOut{ 0 });
            }
        }

        std::ignore = inSpan.consume(to_consume);
        outSpan.publish(to_consume * d_interpolation);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::InterpolatingFirFilter, in, out);

#endif // _GR4_PACKET_MODEM_INTERPOLATING_FIR_FILTER
