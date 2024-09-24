#include <gnuradio-4.0/packet-modem/packet_counter.hpp>

#include "register_helpers.hpp"

void register_packet_counter()
{
    using namespace gr::packet_modem;
    register_all_types<PacketCounter>();
}
