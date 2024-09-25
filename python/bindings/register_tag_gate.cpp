#include <gnuradio-4.0/packet-modem/tag_gate.hpp>

#include "register_helpers.hpp"

void register_tag_gate()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<TagGate>();
}
