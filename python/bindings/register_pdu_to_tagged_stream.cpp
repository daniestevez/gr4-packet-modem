#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>

#include "register_helpers.hpp"

void register_pdu_to_tagged_stream()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<PduToTaggedStream>();
}
