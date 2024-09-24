#include <gnuradio-4.0/packet-modem/crc_check.hpp>

#include "register_helpers.hpp"

void register_crc_check()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlock<CrcCheck, uint64_t, uint32_t, uint16_t>(reg);
}
