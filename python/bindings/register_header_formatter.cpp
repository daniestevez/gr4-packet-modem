#include <gnuradio-4.0/packet-modem/header_formatter.hpp>

#include "register_helpers.hpp"

void register_header_formatter()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlock<HeaderFormatter, uint8_t, Pdu<uint8_t>>(reg);
}
