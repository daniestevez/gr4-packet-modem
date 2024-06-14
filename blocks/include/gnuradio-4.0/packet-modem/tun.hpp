#ifndef _GR4_PACKET_MODEM_TUN
#define _GR4_PACKET_MODEM_TUN

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sched.h>
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
    std::string netns_name;

    void open_tun()
    {
        if (tun_name.size() >= IFNAMSIZ) {
            throw gr::exception("tun_name is too long");
        }

        int original_netns = -1;

        if (!netns_name.empty()) {
            const auto path = fmt::format("/proc/self/task/{}/ns/net", gettid());
            original_netns = open(path.c_str(), O_RDONLY);
            if (original_netns < 0) {
                throw gr::exception(
                    fmt::format("could not open {}: {}", path, strerror(errno)));
            }

            const auto path_new = fmt::format("/var/run/netns/{}", netns_name);
            const int new_ns = open(path_new.c_str(), O_RDONLY);
            if (new_ns < 0) {
                throw gr::exception(
                    fmt::format("could not open {}: {}", path, strerror(errno)));
            }
            if (setns(new_ns, CLONE_NEWNET) < 0) {
                throw gr::exception(fmt::format("setns() failed: {}", strerror(errno)));
            };
            close(new_ns);
        }

        _tun_fd = open("/dev/net/tun", O_RDWR);

        if (original_netns != -1) {
            if (setns(original_netns, CLONE_NEWNET) < 0) {
                throw gr::exception(fmt::format("setns() failed: {}", strerror(errno)));
            };
            close(original_netns);
        }

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
