#include <gnuradio-4.0/packet-modem/header_fec_encoder.hpp>

#include "register_helpers.hpp"

void register_header_fec_encoder()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlock<HeaderFecEncoder, uint8_t, Pdu<uint8_t>>(reg);
}
