#include <gnuradio-4.0/packet-modem/header_fec_decoder.hpp>

#include "register_helpers.hpp"

void register_header_fec_decoder()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<HeaderFecDecoder>("gr::packet_modem::HeaderFecDecoder", "");
}
