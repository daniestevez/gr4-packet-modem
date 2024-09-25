#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>

#include "register_helpers.hpp"

void register_tagged_stream_to_pdu()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<TaggedStreamToPdu>();
}
