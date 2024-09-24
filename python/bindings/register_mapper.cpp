#include <gnuradio-4.0/packet-modem/mapper.hpp>

#include "register_helpers.hpp"

void register_mapper()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlockTT<Mapper,
                    std::tuple<uint64_t, uint32_t, uint16_t, uint8_t>,
                    std::tuple<std::complex<double>,
                               std::complex<float>,
                               double,
                               float,
                               int64_t,
                               int32_t,
                               int16_t,
                               int8_t,
                               uint64_t,
                               uint32_t,
                               uint16_t,
                               uint8_t>>(reg);
}
