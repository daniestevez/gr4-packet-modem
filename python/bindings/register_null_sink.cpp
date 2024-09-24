#include <gnuradio-4.0/packet-modem/null_sink.hpp>

#include "register_helpers.hpp"

void register_null_sink()
{
    using namespace gr::packet_modem;
    register_all_types<NullSink>();
}
