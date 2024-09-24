#include <gnuradio-4.0/packet-modem/packet_ingress.hpp>

#include "register_helpers.hpp"

void register_packet_ingress()
{
    using namespace gr::packet_modem;
    register_all_types<PacketIngress>();
}
