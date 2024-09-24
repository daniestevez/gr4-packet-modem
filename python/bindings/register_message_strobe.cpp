#include <gnuradio-4.0/packet-modem/message_strobe.hpp>
#include <chrono>

#include "register_helpers.hpp"

void register_message_strobe()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<MessageStrobe<std::chrono::system_clock>>(
        "gr::packet_modem::MessageStrobe", "std::chrono::system_clock");
}
