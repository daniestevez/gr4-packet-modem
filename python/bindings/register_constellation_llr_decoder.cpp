#include <gnuradio-4.0/packet-modem/constellation_llr_decoder.hpp>

#include "register_helpers.hpp"

void register_constellation_llr_decoder()
{
    using namespace gr::packet_modem;
    register_all_float_types<ConstellationLLRDecoder>();
}
