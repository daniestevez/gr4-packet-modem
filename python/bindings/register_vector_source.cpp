#include <gnuradio-4.0/packet-modem/vector_source.hpp>

#include "register_helpers.hpp"

void register_vector_source()
{
    using namespace gr::packet_modem;
    register_all_types<VectorSource>();
}
