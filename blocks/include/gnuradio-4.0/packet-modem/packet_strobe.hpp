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
setting and they are filled with zeros. The packets are produced with the
interval indicated in `interval`. If the `packet_len_tag_key` setting is set to a
non-empty string, then a packet length tags will be inserted in the output to
make it a tagged stream.

By default, the block sleeps when it needs to wait to produce a packet. It is
possible to make the block return to the scheduler instead by using `sleep =
false`. This is not recommended because the scheduler will call the block's
`processBulk()` repeatedly without waiting, using a lot of CPU.

)"">;

private:
    using time_point = ClockSourceType::time_point;

    std::optional<gr::property_map> _packet_len_tag;
    size_t _position;
    std::optional<time_point> _last_packet_time;

public:
    gr::PortOut<T> out;
    uint64_t packet_len = 1;
    double interval_secs = 1.0;
    std::string packet_len_tag_key = "";
    bool sleep = true;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        _packet_len_tag =
            packet_len_tag_key.empty()
                ? std::nullopt
                : std::optional<gr::property_map>{
                      { { packet_len_tag_key, static_cast<uint64_t>(packet_len) } }
                  };
    }

    void start() { _position = 0; }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {}), _position = {}, "
                     "_last_packet_time = {}, now = {}",
                     this->name,
                     outSpan.size(),
                     _position,
                     _last_packet_time,
                     ClockSourceType::now());
#endif
        // check if we can produce a new packet now
        if (_position == 0 && _last_packet_time.has_value()) {
            const auto elapsed = ClockSourceType::now() - *_last_packet_time;
            const auto _interval = std::chrono::duration<double>(interval_secs);
            if (elapsed < _interval) {
                if (sleep) {
                    std::this_thread::sleep_for(_interval - elapsed);
                } else {
                    // we cannot produce a packet and cannot sleep, so we return
                    // to the scheduler without any produced items
                    outSpan.publish(0);
                    return gr::work::Status::OK;
                }
            }
        }

        if (_position == 0 && _packet_len_tag.has_value()) {
#ifdef TRACE
            fmt::println("{} publishing packet length tag (now = {})",
                         this->name,
                         ClockSourceType::now());
#endif
            out.publishTag(*_packet_len_tag, 0);
        }

        const auto n = std::min(packet_len - _position, outSpan.size());
        std::ranges::fill(outSpan | std::views::take(n), T{ 0 });
        outSpan.publish(n);
        _position += n;
        if (_position == packet_len) {
            _position = 0;
            _last_packet_time = ClockSourceType::now();
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketStrobe,
                               out,
                               packet_len,
                               interval_secs,
                               packet_len_tag_key,
                               sleep);

#endif // _GR4_PACKET_MODEM_PACKET_STROBE
