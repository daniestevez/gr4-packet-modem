#include <gnuradio-4.0/packet-modem/test_complex_settings.hpp>

#include "register_helpers.hpp"

void register_test_complex_settings()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<TestComplexSettings>();
}
