#include <gnuradio-4.0/packet-modem/message_debug.hpp>

#include "register_helpers.hpp"

void register_message_debug()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<MessageDebug>("gr::packet_modem::MessageDebug", "");
}
