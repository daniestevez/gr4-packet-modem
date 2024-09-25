#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>

#include "register_helpers.hpp"

void register_syncword_detection()
{
    using namespace gr::packet_modem;
    auto& reg = gr::globalBlockRegistry();
    reg.addBlockType<SyncwordDetection>("gr::packet_modem::SyncwordDetection", "");
}
