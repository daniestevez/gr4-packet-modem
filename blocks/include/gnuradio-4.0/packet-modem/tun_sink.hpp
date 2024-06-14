#ifndef _GR4_PACKET_MODEM_TUN_SINK
#define _GR4_PACKET_MODEM_TUN_SINK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/tun.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <cerrno>

namespace gr::packet_modem {

class TunSink : public gr::Block<TunSink, gr::BlockingIO<true>>, public TunBlock
{
public:
    using Description = Doc<R""(
@brief TUN Sink. Writes IP packets to a TUN device.

)"">;

public:
    gr::PortIn<Pdu<uint8_t>> in;

    void start() { open_tun(); }

    void stop() { close_tun(); }

    void processOne(const Pdu<uint8_t>& a)
    {
        const size_t size = a.data.size();
        const ssize_t ret = write(_tun_fd, a.data.data(), size);
        if (ret != static_cast<ssize_t>(size)) {
            // do not throw an exception, because trying to write badly
            // formatted packets (as a consequence of a wrong decode) or
            // with the TUN down gives an error
            fmt::println("{} write to TUN failed: {}", this->name, strerror(errno));
        }
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::TunSink, in, tun_name);

#endif // _GR4_PACKET_MODEM_TUN_SINK
