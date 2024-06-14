#ifndef _GR4_PACKET_MODEM_TUN
#define _GR4_PACKET_MODEM_TUN

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace gr::packet_modem {

class TunBlock
{
public:
    int _tun_fd;
    std::string tun_name;

    void open_tun()
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

    void close_tun() { close(_tun_fd); }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_TUN
