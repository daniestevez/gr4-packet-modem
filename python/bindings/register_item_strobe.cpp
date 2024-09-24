#include <gnuradio-4.0/packet-modem/item_strobe.hpp>
#include <chrono>

#include "register_helpers.hpp"

namespace gr::packet_modem {
template <typename T>
using ItemStrobeSystemClock = ItemStrobe<T, std::chrono::system_clock>;
}

void register_item_strobe()
{
    using namespace gr::packet_modem;
    register_all_types<ItemStrobeSystemClock>();
}
