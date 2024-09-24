#include <gnuradio-4.0/packet-modem/costas_loop.hpp>

#include "register_helpers.hpp"

void register_costas_loop()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    registerBlockTT<CostasLoop, std::tuple<double, float>, std::tuple<double, float>>(
        reg);
}
