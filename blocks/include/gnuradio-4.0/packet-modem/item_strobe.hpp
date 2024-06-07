#ifndef _GR4_PACKET_MODEM_ITEM_STROBE
#define _GR4_PACKET_MODEM_ITEM_STROBE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <chrono>
#include <optional>
#include <ranges>

namespace gr::packet_modem {

template <typename T, typename ClockSourceType = std::chrono::system_clock>
class ItemStrobe : public gr::Block<ItemStrobe<T, ClockSourceType>>
{
public:
    using Description = Doc<R""(
@brief Item strobe. Produces one item periodically.

The items are produced with the interval indicated in `interval`.  By default,
the block sleeps when it needs to wait to produce an item. It is possible to
make the block return to the scheduler instead by using `sleep = false`. This is
not recommended because the scheduler will call the block's `processBulk()`
repeatedly without waiting, using a lot of CPU.

)"">;

private:
    using time_point = ClockSourceType::time_point;

public:
    std::optional<time_point> _last_item_time;

public:
    gr::PortOut<T, gr::RequiredSamples<1U, 1U>> out;
    T item;
    double interval_secs = 1.0;
    bool sleep = true;

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {}), "
                     "_last_item_time = {}, now = {}",
                     this->name,
                     outSpan.size(),
                     _last_item_time,
                     ClockSourceType::now());
#endif
        // check if we can produce a new item now
        if (_last_item_time.has_value()) {
            const auto elapsed = ClockSourceType::now() - *_last_item_time;
            const auto _interval = std::chrono::duration<double>(interval_secs);
            if (elapsed < _interval) {
                if (sleep) {
                    std::this_thread::sleep_for(_interval - elapsed);
                } else {
                    // we cannot produce an item and cannot sleep, so we return
                    // to the scheduler without any produced items
                    outSpan.publish(0);
                    return gr::work::Status::OK;
                }
            }
        }

        outSpan[0] = item;
        outSpan.publish(1);
        _last_item_time = ClockSourceType::now();

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::ItemStrobe, out, interval_secs, sleep);

#endif // _GR4_PACKET_MODEM_ITEM_STROBE
