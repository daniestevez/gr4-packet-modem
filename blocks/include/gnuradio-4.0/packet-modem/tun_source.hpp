#ifndef _GR4_PACKET_MODEM_TUN_SOURCE
#define _GR4_PACKET_MODEM_TUN_SOURCE

#include <gnuradio-4.0/Block.hpp>
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

)"">;

public:
    std::array<uint8_t, 65536> _buff;

public:
    gr::PortOut<Pdu<uint8_t>, gr::RequiredSamples<1U, 1U>> out;
    double timeout = 1.0;

    void start() { open_tun(); }

    void stop() { close_tun(); }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, outSpan.size());
#endif
        fd_set rfds;
        struct timeval _timeout;
        FD_ZERO(&rfds);
        FD_SET(_tun_fd, &rfds);
        _timeout.tv_sec = static_cast<int>(timeout);
        _timeout.tv_usec = static_cast<uint32_t>(
            std::round((timeout - static_cast<double>(_timeout.tv_sec)) * 1e6));
        ssize_t ret = select(_tun_fd + 1, &rfds, nullptr, nullptr, &_timeout);
        if (ret < 0) {
            throw gr::exception(fmt::format("select() failed: {}", strerror(errno)));
        } else if (ret == 0) {
            // timeout expired; return to the scheduler
            outSpan.publish(0);
            return gr::work::Status::OK;
        }

        // select() says that the TUN has data; read it
        ret = read(_tun_fd, _buff.data(), sizeof(_buff));
        if (ret < 0) {
            throw gr::exception(fmt::format("read failed: {}", strerror(errno)));
        }
        outSpan[0].data.reserve(static_cast<size_t>(ret));
        outSpan[0].data.clear();
        std::copy_n(_buff.cbegin(), ret, std::back_inserter(outSpan[0].data));
        outSpan[0].tags.clear();
        outSpan.publish(1);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::TunSource, out, tun_name, netns_name, timeout);

#endif // _GR4_PACKET_MODEM_TUN_SOURCE
