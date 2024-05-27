/*
 * This file contains code adapted from GNU Radio 3.10, which is licensed as
 * follows:
 *
 * Copyright 2005-2011 Free Software Foundation, Inc.
 * Copyright 2022 Marcus Müller
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

#ifndef _GR4_PACKET_MODEM_THROTTLE
#define _GR4_PACKET_MODEM_THROTTLE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <chrono>
#include <ranges>

namespace gr::packet_modem {

template <typename T, typename ClockSourceType = std::chrono::steady_clock>
class Throttle : public gr::Block<Throttle<T, ClockSourceType>, gr::BlockingIO<true>>
{
public:
    using Description = Doc<R""(
@brief Throttle block. Limits the rate at which items flow through this block.

This block limits the rate at which items flow through it by counting the number
of items and the time that has passed since the block was started and only
returning from the `processBulk()` function when enough time has elapsed
according to the indicated `sample_rate`.

Optionally, the block can limit the number of items that it copies from the
input to the output in a single call to `processBulk()` in order to avoid
spending too long sleeping in `processBulk()` at low sample rates (which can
cause a large latency). To use this feature, set the `maximum_items_per_chunk`
parameter to a non-zero value that indicates the maximum number of items that
`processBulk()` is allowed to publish.

)"">;

private:
    const double d_sample_rate;
    const std::chrono::duration<double> d_sample_period;
    const size_t d_maximum_items_per_chunk;
    uint64_t d_total_items;
    ClockSourceType::time_point d_start;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    Throttle(double sample_rate, size_t maximum_items_per_chunk = 0)
        : d_sample_rate(sample_rate),
          d_sample_period(1.0 / sample_rate),
          d_maximum_items_per_chunk(maximum_items_per_chunk)
    {
    }

    void start()
    {
        d_total_items = 0;
        d_start = ClockSourceType::now();
    }

    void stop()
    {
        // this seems to be necessary for a BlockIO block (see clock_source.hpp
        // in gnuradio4)
        this->requestStop();
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

        size_t items_per_chunk = std::min(inSpan.size(), outSpan.size());
        if (d_maximum_items_per_chunk) {
            items_per_chunk = std::min(items_per_chunk, d_maximum_items_per_chunk);
        }

        std::ranges::copy_n(
            inSpan.begin(), static_cast<ssize_t>(items_per_chunk), outSpan.begin());

        const auto now = ClockSourceType::now();
        const auto expected_time =
            d_start + d_sample_period * (d_total_items + items_per_chunk);
        if (expected_time > now) {
            const auto duration = expected_time - now;
            constexpr auto limit_duration =
                std::chrono::duration<double>(std::numeric_limits<long>::max());
            if (duration > limit_duration) {
                fmt::println("{} WARNING: Throttle sleep time overflow! You are probably "
                             "using a very low sample rate.",
                             this->name);
            }
            std::this_thread::sleep_for(duration);
        }

        d_total_items += items_per_chunk;
        std::ignore = inSpan.consume(items_per_chunk);
        outSpan.publish(items_per_chunk);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Throttle, in, out);

#endif // _GR4_PACKET_MODEM_THROTTLE
