#include <gnuradio-4.0/packet-modem/rotator.hpp>

#include "register_helpers.hpp"

void register_rotator()
{
    using namespace gr::packet_modem;
    register_all_float_types<Rotator>();
}
