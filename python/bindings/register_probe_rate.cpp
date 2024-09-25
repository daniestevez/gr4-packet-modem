#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <chrono>

#include "register_helpers.hpp"

namespace gr::packet_modem {
template <typename T>
using ProbeRateSteadyClock = ProbeRate<T, std::chrono::steady_clock>;
}

void register_probe_rate()
{
    using namespace gr::packet_modem;
    register_all_types<ProbeRateSteadyClock>();
}
