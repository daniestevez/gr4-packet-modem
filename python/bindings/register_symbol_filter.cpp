#include <gnuradio-4.0/packet-modem/symbol_filter.hpp>

#include "register_helpers.hpp"

void register_symbol_filter()
{
    using namespace gr;
    using namespace gr::packet_modem;
    registerBlock<
        SymbolFilter,
        BlockParameters<float, float, float>,
        BlockParameters<std::complex<float>, std::complex<float>, std::complex<float>>,
        BlockParameters<std::complex<float>, std::complex<float>, float>,
        BlockParameters<float, std::complex<float>, std::complex<float>>>(
        globalBlockRegistry());
}
