#include <gnuradio-4.0/packet-modem/syncword_wipeoff.hpp>
#include <complex>

#include "register_helpers.hpp"

void register_syncword_wipeoff()
{
    using namespace gr;
    using namespace gr::packet_modem;
    registerBlock<SyncwordWipeoff,
                  BlockParameters<std::complex<float>, float>,
                  BlockParameters<float, float>,
                  BlockParameters<std::complex<double>, double>,
                  BlockParameters<double, double>>(globalBlockRegistry());
}
