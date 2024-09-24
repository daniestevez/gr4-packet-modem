#include <gnuradio-4.0/packet-modem/coarse_frequency_correction.hpp>

#include "register_helpers.hpp"

void register_coarse_frequency_correction()
{
    using namespace gr::packet_modem;
    register_all_float_types<CoarseFrequencyCorrection>();
}
