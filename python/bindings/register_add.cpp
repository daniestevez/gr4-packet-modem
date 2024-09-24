#include <gnuradio-4.0/packet-modem/add.hpp>

#include "register_helpers.hpp"

void register_add()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<Add>();
}
