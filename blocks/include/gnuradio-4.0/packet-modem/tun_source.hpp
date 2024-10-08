#ifndef _GR4_PACKET_MODEM_TUN_SOURCE
#define _GR4_PACKET_MODEM_TUN_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/tun.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <sys/select.h>
#include <array>
#include <cerrno>
#include <ranges>

namespace gr::packet_modem {

class TunSource : public gr::Block<TunSource, gr::BlockingIO<true>>, public TunBlock
{
public:
    using Description = Doc<R""(
@brief TUN Source. Reads IP packets from a TUN device.

The TUN Source block includes a latency management system. See Packet Limiter
for reference.

)"">;

public:
    std::array<uint8_t, 65536> _buff;
    uint64_t _exit_count;
    uint64_t _entry_count;

public:
    gr::PortIn<gr::Message, gr::Async> count;
    gr::PortOut<Pdu<uint8_t>, gr::RequiredSamples<1U, 1U>> out;
    double timeout = 1.0;
    size_t max_packets = 0UZ;
    size_t idle_packet_size = 0UZ; // 0 means do not generate idle packets

    void start()
    {
        _entry_count = 0;
        _exit_count = 0;
        open_tun();
    }

    void stop() { close_tun(); }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& countSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(countSpan.size() = {}, outSpan.size() = {}), "
                     "_entry_count = {}, _exit_count = {}",
                     this->name,
                     countSpan.size(),
                     outSpan.size(),
                     _entry_count,
                     _exit_count);
#endif

        if (countSpan.size() > 0) {
            const auto& msg = countSpan[countSpan.size() - 1];
            _exit_count = pmtv::cast<uint64_t>(msg.data.value().at("packet_count"));
#ifdef TRACE
            fmt::println("{} updated _exit_count = {}", this->name, _exit_count);
#endif
            if (!countSpan.consume(countSpan.size())) {
                throw gr::exception("consume failed");
            }
        }

        if (max_packets > 0 &&
            _entry_count - _exit_count >= static_cast<uint64_t>(max_packets)) {
            // cannot allow more packets into the latency management region
#ifdef TRACE
            fmt::println("{} latency management region full", this->name);
#endif
            outSpan.publish(0);
            return gr::work::Status::OK;
        }

        fd_set rfds;
        fd_set rfds_err;
        struct timeval _timeout;
        FD_ZERO(&rfds);
        FD_SET(_tun_fd, &rfds);
        FD_ZERO(&rfds_err);
        FD_SET(_tun_fd, &rfds_err);
        if (idle_packet_size > 0) {
            // use a timeout of 0 when idle packet generation is enabled
            _timeout.tv_sec = 0;
            _timeout.tv_usec = 0;
        } else {
            _timeout.tv_sec = static_cast<int>(timeout);
            _timeout.tv_usec = static_cast<uint32_t>(
                std::round((timeout - static_cast<double>(_timeout.tv_sec)) * 1e6));
        }
        ssize_t ret = select(_tun_fd + 1, &rfds, nullptr, &rfds_err, &_timeout);
        if (ret < 0) {
            throw gr::exception(fmt::format("select() failed: {}", strerror(errno)));
        } else if (ret == 0) {
            // a packet is not available in the TUN
            if (idle_packet_size > 0) {
                // generate idle packet
                outSpan[0].data = std::vector<uint8_t>(idle_packet_size);
                outSpan[0].tags =
                    std::vector<gr::Tag>{ { 0, { { "packet_type", "IDLE" } } } };
#ifdef TRACE
                fmt::println("{} produced IDLE packet", this->name);
#endif
            } else {
                // timeout expired; return to the scheduler without a packet
#ifdef TRACE
                fmt::println("{} timeout expired; returning to scheduler", this->name);
#endif
                outSpan.publish(0);
                return gr::work::Status::OK;
            }
        } else {
            // select() says that the TUN has data; read it
            ret = read(_tun_fd, _buff.data(), sizeof(_buff));
            if (ret < 0) {
                throw gr::exception(fmt::format("read failed: {}", strerror(errno)));
            }
            outSpan[0].data.reserve(static_cast<size_t>(ret));
            outSpan[0].data.clear();
            std::copy_n(_buff.cbegin(), ret, std::back_inserter(outSpan[0].data));
            outSpan[0].tags.clear();
#ifdef TRACE
            fmt::println("{} produced non-IDLE packet", this->name);
#endif
        }

        // this is run both when generating an idle packet and when generating a
        // packet from the TUN
        outSpan.publish(1);
        ++_entry_count;

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::TunSource,
                  count,
                  out,
                  tun_name,
                  netns_name,
                  timeout,
                  max_packets,
                  idle_packet_size);

#endif // _GR4_PACKET_MODEM_TUN_SOURCE
