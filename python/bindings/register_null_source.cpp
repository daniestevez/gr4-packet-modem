#include <gnuradio-4.0/packet-modem/null_source.hpp>

#include "register_helpers.hpp"

void register_null_source()
{
    using namespace gr::packet_modem;
    register_all_types<NullSource>();
}
