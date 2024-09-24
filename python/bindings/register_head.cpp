#include <gnuradio-4.0/packet-modem/head.hpp>

#include "register_helpers.hpp"

void register_head()
{
    using namespace gr::packet_modem;
    register_all_types<Head>();
}
