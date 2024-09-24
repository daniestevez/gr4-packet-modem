#include <gnuradio-4.0/packet-modem/file_source.hpp>

#include "register_helpers.hpp"

void register_file_source()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<FileSource>();
}
