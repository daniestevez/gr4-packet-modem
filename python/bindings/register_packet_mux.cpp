#include <gnuradio-4.0/packet-modem/packet_mux.hpp>

#include "register_helpers.hpp"

void register_packet_mux()
{
    using namespace gr::packet_modem;
    register_all_types<PacketMux>();
}
