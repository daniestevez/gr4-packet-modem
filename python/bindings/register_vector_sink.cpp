#include <gnuradio-4.0/packet-modem/vector_sink.hpp>

#include "register_helpers.hpp"

void register_vector_sink()
{
    using namespace gr::packet_modem;
    register_all_types<VectorSink>();
}
