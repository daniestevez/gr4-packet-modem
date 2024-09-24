#include <gnuradio-4.0/packet-modem/crc_append.hpp>

#include "register_helpers.hpp"

void register_crc_append()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlockTT<CrcAppend,
                    std::tuple<uint8_t, Pdu<uint8_t>>,
                    std::tuple<uint64_t, uint32_t, uint16_t>>(reg);
}
