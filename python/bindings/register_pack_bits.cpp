#include <gnuradio-4.0/packet-modem/pack_bits.hpp>

#include "register_helpers.hpp"

void register_pack_bits()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlockVTT<PackBits,
                     Endianness::MSB,
                     std::tuple<uint64_t, uint32_t, uint16_t, uint8_t>,
                     std::tuple<uint64_t, uint32_t, uint16_t, uint8_t>>(reg);
}
