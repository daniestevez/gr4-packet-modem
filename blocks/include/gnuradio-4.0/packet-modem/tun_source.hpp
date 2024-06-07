#ifndef _GR4_PACKET_MODEM_TUN_SOURCE
#define _GR4_PACKET_MODEM_TUN_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <ranges>

namespace gr::packet_modem {

class TunSource : public gr::Block<TunSource, gr::BlockingIO<true>>
{
public:
    using Description = Doc<R""(
@brief TUN Source. Reads IP packets from a TUN device.

)"">;

public:
    int _tun_fd;
    std::array<uint8_t, 65536> _buff;

public:
    gr::PortOut<Pdu<uint8_t>, gr::RequiredSamples<1U, 1U>> out;
    std::string tun_name;

    void start()
    {
        if (tun_name.size() >= IFNAMSIZ) {
            throw gr::exception("tun_name is too long");
        }
        _tun_fd = open("/dev/net/tun", O_RDWR);
        if (_tun_fd < 0) {
            throw gr::exception(
                fmt::format("could not open /dev/net/tun: {}", strerror(errno)));
        }
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
        memcpy(ifr.ifr_name, tun_name.c_str(), tun_name.size() + 1);
        if (ioctl(_tun_fd, TUNSETIFF, static_cast<void*>(&ifr)) < 0) {
            throw gr::exception(fmt::format("ioctl failed: {}", strerror(errno)));
        }
    }

    void stop() { close(_tun_fd); }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, outSpan.size());
#endif
        ssize_t ret = read(_tun_fd, _buff.data(), sizeof(_buff));
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

ENABLE_REFLECTION(gr::packet_modem::TunSource, out, tun_name);

#endif // _GR4_PACKET_MODEM_TUN_SOURCE
