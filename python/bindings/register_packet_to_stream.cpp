#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>

#include "register_helpers.hpp"

void register_packet_to_stream()
{
    using namespace gr::packet_modem;
    register_all_types<PacketToStream>();
}
