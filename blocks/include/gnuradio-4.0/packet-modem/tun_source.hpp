#ifndef _GR4_PACKET_MODEM_TUN_SOURCE
#define _GR4_PACKET_MODEM_TUN_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/tun.hpp>
#include <gnuradio-4.0/reflection.hpp>
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

    void start() { open_tun(); }

    void stop() { close_tun(); }

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
