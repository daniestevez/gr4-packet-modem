#include <gnuradio-4.0/packet-modem/packet_limiter.hpp>

#include "register_helpers.hpp"

void register_packet_limiter()
{
    using namespace gr::packet_modem;
    register_all_pdu_types<PacketLimiter>();
}
