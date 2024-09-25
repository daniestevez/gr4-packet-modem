#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <chrono>

#include "register_helpers.hpp"

namespace gr::packet_modem {
template <typename T>
using ThrottleSteadyClock = Throttle<T, std::chrono::steady_clock>;
}

void register_throttle()
{
    using namespace gr::packet_modem;
    register_all_types<ThrottleSteadyClock>();
}
