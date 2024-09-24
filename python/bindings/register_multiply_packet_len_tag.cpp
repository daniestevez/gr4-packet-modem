#include <gnuradio-4.0/packet-modem/multiply_packet_len_tag.hpp>

#include "register_helpers.hpp"

void register_multiply_packet_len_tag()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<MultiplyPacketLenTag>();
}
