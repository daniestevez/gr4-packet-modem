#include <gnuradio-4.0/packet-modem/header_payload_split.hpp>

#include "register_helpers.hpp"

void register_header_payload_split()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<HeaderPayloadSplit>();
}
