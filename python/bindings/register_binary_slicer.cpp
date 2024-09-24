#include <gnuradio-4.0/packet-modem/binary_slicer.hpp>

#include "register_helpers.hpp"

void register_binary_slicer()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlockVTT<BinarySlicer,
                     false,
                     std::tuple<double, float, int64_t, int32_t, int16_t, int8_t>,
                     std::tuple<uint64_t, uint32_t, uint16_t, uint8_t>>(reg);
    registerBlockVTT<BinarySlicer,
                     true,
                     std::tuple<double, float, int64_t, int32_t, int16_t, int8_t>,
                     std::tuple<uint64_t, uint32_t, uint16_t, uint8_t>>(reg);
}
