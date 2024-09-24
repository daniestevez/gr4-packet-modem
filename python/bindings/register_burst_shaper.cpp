#include <gnuradio-4.0/packet-modem/burst_shaper.hpp>

#include "register_helpers.hpp"

void register_burst_shaper()
{
    using namespace gr::packet_modem;
    register_scalar_mult<BurstShaper>();
}
