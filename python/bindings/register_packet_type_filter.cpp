#include <gnuradio-4.0/packet-modem/packet_type_filter.hpp>

#include "register_helpers.hpp"

void register_packet_type_filter()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<PacketTypeFilter>();
}
