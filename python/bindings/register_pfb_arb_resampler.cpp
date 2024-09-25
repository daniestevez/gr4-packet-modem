#include <gnuradio-4.0/packet-modem/pfb_arb_resampler.hpp>

#include "register_helpers.hpp"

void register_pfb_arb_resampler()
{
    using namespace gr;
    using namespace gr::packet_modem;
    registerBlock<
        PfbArbResampler,
        BlockParameters<float, float, float, float>,
        BlockParameters<std::complex<float>,
                        std::complex<float>,
                        std::complex<float>,
                        float>,
        BlockParameters<std::complex<float>, std::complex<float>, float, float>,
        BlockParameters<float, std::complex<float>, std::complex<float>, float>,
        BlockParameters<double, double, double, float>,
        BlockParameters<std::complex<double>,
                        std::complex<double>,
                        std::complex<double>,
                        float>,
        BlockParameters<std::complex<double>, std::complex<double>, double, float>,
        BlockParameters<double, std::complex<double>, std::complex<double>, float>,
        BlockParameters<float, float, float, double>,
        BlockParameters<std::complex<float>,
                        std::complex<float>,
                        std::complex<float>,
                        double>,
        BlockParameters<std::complex<float>, std::complex<float>, float, double>,
        BlockParameters<float, std::complex<float>, std::complex<float>, double>,
        BlockParameters<double, double, double, double>,
        BlockParameters<std::complex<double>,
                        std::complex<double>,
                        std::complex<double>,
                        double>,
        BlockParameters<std::complex<double>, std::complex<double>, double, double>,
        BlockParameters<double, std::complex<double>, std::complex<double>, double>>(
        gr::globalBlockRegistry());
}
