#include <gnuradio-4.0/packet-modem/header_parser.hpp>

#include "register_helpers.hpp"

void register_header_parser()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlock<HeaderParser, uint8_t>(reg);
}
