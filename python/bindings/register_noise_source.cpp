#include <gnuradio-4.0/packet-modem/noise_source.hpp>

#include "register_helpers.hpp"

void register_noise_source()
{
    using namespace gr::packet_modem;
    register_all_float_and_complex_types<NoiseSource>();
}
