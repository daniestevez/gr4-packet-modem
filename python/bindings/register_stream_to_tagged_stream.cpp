#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>

#include "register_helpers.hpp"

void register_stream_to_tagged_stream()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<StreamToTaggedStream>();
}
