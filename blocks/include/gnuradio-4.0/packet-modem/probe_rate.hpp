#ifndef _GR4_PACKET_MODEM_PROBE_RATE
#define _GR4_PACKET_MODEM_PROBE_RATE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

namespace gr::packet_modem {

template <typename T, typename ClockSourceType = std::chrono::steady_clock>
class ProbeRate : public gr::Block<ProbeRate<T, ClockSourceType>>
{
public:
    using Description = Doc<R""(
@brief Probe Rate. Measures the rate at which items are consumed by this block.

This block acts as a sink, counting the number of items that it
consumes. Periodically, as indicated by the `min_update_time` parameter, it
generates an output message that contains the rate at which items are being
sinked currently (computed as the different quotient of two item counts spaced
by the update time), and the average rate, which is the rate smoothed with a
1-pole IIR filter.

)"">;

private:
    const ClockSourceType::duration d_min_update_time;
    const double d_alpha;
    std::atomic<uint64_t> d_samples_consumed;
    std::condition_variable d_cv;
    bool d_stop;
    std::mutex d_mutex;
    std::thread d_thread;

    void start_thread()
    {
        {
            std::lock_guard lock(d_mutex);
            d_stop = false;
        }
        d_thread = std::thread([this]() {
            auto start = ClockSourceType::now();
            auto count_start = d_samples_consumed.load(std::memory_order::relaxed);
            std::optional<double> rate_avg = std::nullopt;
            while (true) {
                {
                    std::unique_lock lock(d_mutex);
                    const auto wakeup = start + d_min_update_time;
                    // Sleep for d_min_update_time or until stop is set to true and the
                    // condition variable is notified.
                    d_cv.wait_for(lock, d_min_update_time, [this, wakeup]() {
                        return d_stop || ClockSourceType::now() >= wakeup;
                    });
                    if (d_stop) {
                        return;
                    }
                }
                const auto end = ClockSourceType::now();
                const auto count_end =
                    d_samples_consumed.load(std::memory_order::relaxed);
                const double rate_now =
                    static_cast<double>(count_end - count_start) /
                    std::chrono::duration<double>(end - start).count();
                if (rate_avg.has_value()) {
                    rate_avg = (1.0 - d_alpha) * *rate_avg + d_alpha * rate_now;
                } else {
                    rate_avg = rate_now;
                }

                // Use gr::message::Command::Invalid, since none of the OpenCMW
                // commands is appropriate for GNU Radio 3.10-style message passing.
                gr::sendMessage<gr::message::Command::Invalid>(
                    rate,
                    "",
                    "",
                    { { "rate_now", rate_now }, { "rate_avg", *rate_avg } });

                start = end;
                count_start = count_end;
            }
        });
    }

    void stop_thread()
    {
        {
            std::lock_guard lock(d_mutex);
            d_stop = true;
        }
        d_cv.notify_one();
        d_thread.join();
    }

public:
    gr::PortIn<T> in;
    gr::MsgPortOut rate;

    ProbeRate(ClockSourceType::duration min_update_time, double update_alpha = 0.15)
        : d_min_update_time(min_update_time), d_alpha(update_alpha), d_samples_consumed(0)
    {
    }

    void start() { start_thread(); }

    void stop() { stop_thread(); }

    void pause() { stop_thread(); }

    void resume() { start_thread(); }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan)
    {
#ifdef TRACE
        fmt::print("ProbeRate::processBulk(inSpan.size() = {})\n", inSpan.size());
#endif
        const auto n = inSpan.size();
        d_samples_consumed.fetch_add(static_cast<uint64_t>(n),
                                     std::memory_order::relaxed);
        std::ignore = inSpan.consume(n);
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::ProbeRate, in);

#endif // _GR4_PACKET_MODEM_PROBE_RATE
