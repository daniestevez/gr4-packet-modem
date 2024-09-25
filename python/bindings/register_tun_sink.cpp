#include <gnuradio-4.0/packet-modem/tun_sink.hpp>

#include "register_helpers.hpp"

void register_tun_sink()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<TunSink>("gr::packet_modem::TunSink", "");
}
