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
consumes. Periodically, as indicated by the `min_update_time_secs` parameter, it
generates an output message that contains the rate at which items are being
sinked currently (computed as the different quotient of two item counts spaced
by the update time), and the average rate, which is the rate smoothed with a
1-pole IIR filter.

)"">;

public:
    std::atomic<uint64_t> _samples_consumed;
    std::condition_variable _cv;
    bool _stop;
    std::mutex _mutex;
    std::thread _thread;

private:
    void start_thread()
    {
        _stop = false;
        _thread = std::thread([this]() {
            auto start = ClockSourceType::now();
            auto count_start = _samples_consumed.load(std::memory_order::relaxed);
            std::optional<double> rate_avg = std::nullopt;
            while (true) {
                {
                    std::unique_lock lock(_mutex);
                    const auto update_time =
                        std::chrono::duration<double>(min_update_time_secs);
                    const auto wakeup = start + update_time;
                    // Sleep for update_time or until stop is set to true and the
                    // condition variable is notified.
                    _cv.wait_for(lock, update_time, [this, wakeup]() {
                        return _stop || ClockSourceType::now() >= wakeup;
                    });
                    if (_stop) {
                        return;
                    }
                }
                const auto end = ClockSourceType::now();
                const auto count_end = _samples_consumed.load(std::memory_order::relaxed);
                const double rate_now =
                    static_cast<double>(count_end - count_start) /
                    std::chrono::duration<double>(end - start).count();
                if (rate_avg.has_value()) {
                    rate_avg = (1.0 - alpha) * *rate_avg + alpha * rate_now;
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
            std::lock_guard lock(_mutex);
            _stop = true;
        }
        _cv.notify_one();
        _thread.join();
    }

public:
    gr::PortIn<T> in;
    gr::MsgPortOut rate;
    double min_update_time_secs = 1.0;
    double alpha = 0.15;

    void start()
    {
        _samples_consumed.store(0, std::memory_order::relaxed);
        start_thread();
    }

    void stop() { stop_thread(); }

    void pause() { stop_thread(); }

    void resume() { start_thread(); }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {})", this->name, inSpan.size());
#endif
        _samples_consumed.fetch_add(static_cast<uint64_t>(inSpan.size()),
                                    std::memory_order::relaxed);
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::ProbeRate,
                               in,
                               min_update_time_secs,
                               alpha);

#endif // _GR4_PACKET_MODEM_PROBE_RATE
