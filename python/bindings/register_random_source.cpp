#include <gnuradio-4.0/packet-modem/random_source.hpp>

#include "register_helpers.hpp"

void register_random_source()
{
    using namespace gr::packet_modem;
    gr::registerBlock<RandomSource,
                      int64_t,
                      int32_t,
                      int16_t,
                      int8_t,
                      uint64_t,
                      uint32_t,
                      uint16_t,
                      uint8_t>(gr::globalBlockRegistry());
}
