#include <gnuradio-4.0/packet-modem/glfsr_source.hpp>

#include "register_helpers.hpp"

void register_glfsr_source()
{
    using namespace gr::packet_modem;
    register_all_uint_types<GlfsrSource>();
}
