#include <gnuradio-4.0/packet-modem/tun_source.hpp>

#include "register_helpers.hpp"

void register_tun_source()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<TunSource>("gr::packet_modem::TunSource", "");
}
