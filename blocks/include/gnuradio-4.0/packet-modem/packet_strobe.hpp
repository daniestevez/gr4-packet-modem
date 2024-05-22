#ifndef _GR4_PACKET_MODEM_PACKET_STROBE
#define _GR4_PACKET_MODEM_PACKET_STROBE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <chrono>
#include <optional>
#include <ranges>

namespace gr::packet_modem {

template <typename T, typename ClockSourceType = std::chrono::system_clock>
class PacketStrobe : public gr::Block<PacketStrobe<T, ClockSourceType>>
{
public:
    using Description = Doc<R""(
@brief Packet strobe. Produces one packet periodically.

The packets produced by this block have a fixed length given by the `packet_len`
argument and they are filled with zeros. The packets are produced with the
interval indicated in `interval`. If the `packet_len_tag` argument is set to a
non-empty string, then a packet length tags will be inserted in the output to
make it a tagged stream.

By default, the block sleeps when it needs to wait to produce a packet. It is
possible to make the block return to the scheduler instead by using `sleep =
false`. This is not recommended because the scheduler will call the block's
`processBulk()` repeatedly without waiting, using a lot of CPU.

)"">;

private:
    using time_point = ClockSourceType::time_point;

    const size_t d_packet_len;
    const ClockSourceType::duration d_interval;
    const bool d_sleep;
    const std::optional<gr::property_map> d_packet_len_tag;
    size_t d_position = 0;
    std::optional<time_point> d_last_packet_time = std::nullopt;

public:
    gr::PortOut<T> out;

    PacketStrobe(size_t packet_len,
                 ClockSourceType::duration interval,
                 const std::string& packet_len_tag = "",
                 bool sleep = true)
        : d_packet_len(packet_len),
          d_interval(interval),
          d_sleep(sleep),
          d_packet_len_tag(
              packet_len_tag.empty()
                  ? std::nullopt
                  : std::optional<property_map>{
                        { { packet_len_tag, static_cast<uint64_t>(packet_len) } } })
    {
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {}), d_position = {}, "
                     "d_last_packet_time = {}, now = {}",
                     this->name,
                     outSpan.size(),
                     d_position,
                     d_last_packet_time,
                     ClockSourceType::now());
#endif
        // check if we can produce a new packet now
        if (d_position == 0 && d_last_packet_time.has_value()) {
            const auto elapsed = ClockSourceType::now() - *d_last_packet_time;
            if (elapsed < d_interval) {
                if (d_sleep) {
                    std::this_thread::sleep_for(d_interval - elapsed);
                } else {
                    // we cannot produce a packet and cannot sleep, so we return
                    // to the scheduler without any produced items
                    outSpan.publish(0);
                    return gr::work::Status::OK;
                }
            }
        }

        if (d_position == 0 && d_packet_len_tag.has_value()) {
#ifdef TRACE
            fmt::println("{} publishing packet length tag (now = {})",
                         this->name,
                         ClockSourceType::now());
#endif
            out.publishTag(*d_packet_len_tag, 0);
        }

        const auto n = std::min(d_packet_len - d_position, outSpan.size());
        std::ranges::fill(outSpan | std::views::take(n), T{ 0 });
        outSpan.publish(n);
        d_position += n;
        if (d_position == d_packet_len) {
            d_position = 0;
            d_last_packet_time = ClockSourceType::now();
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketStrobe, out);

#endif // _GR4_PACKET_MODEM_PACKET_STROBE
