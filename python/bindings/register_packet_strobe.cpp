#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <chrono>

#include "register_helpers.hpp"

namespace gr::packet_modem {
template <typename T>
using PacketStrobeSystemClock = PacketStrobe<T, std::chrono::system_clock>;
}

void register_packet_strobe()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<PacketStrobeSystemClock>();
}
