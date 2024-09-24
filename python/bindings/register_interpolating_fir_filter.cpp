#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>

#include "register_helpers.hpp"

void register_interpolating_fir_filter()
{
    using namespace gr::packet_modem;
    register_scalar_mult<InterpolatingFirFilter>();
}
