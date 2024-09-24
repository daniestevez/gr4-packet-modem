#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>

#include "register_helpers.hpp"

void register_additive_scrambler()
{
    using namespace gr::packet_modem;
    register_all_types<AdditiveScrambler>();
}
