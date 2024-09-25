#include <gnuradio-4.0/packet-modem/zmq_pdu_pub_sink.hpp>

#include "register_helpers.hpp"

void register_zmq_pdu_pub_sink()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<ZmqPduPubSink>();
}
