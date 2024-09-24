#include <gnuradio-4.0/packet-modem/file_sink.hpp>

#include "register_helpers.hpp"

void register_file_sink()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<FileSink>();
}
