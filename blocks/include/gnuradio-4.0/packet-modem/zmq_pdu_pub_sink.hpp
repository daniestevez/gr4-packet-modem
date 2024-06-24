#ifndef _GR4_PACKET_MODEM_ZMQ_PDU_PUB_SINK
#define _GR4_PACKET_MODEM_ZMQ_PDU_PUB_SINK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <zmq.hpp>
#include <cerrno>

namespace gr::packet_modem {

template <typename T>
class ZmqPduPubSink : public gr::Block<ZmqPduPubSink<T>>
{
public:
    using Description = Doc<R""(
@brief ZMQ PDU PUB Sink.

This block sends PDUs as ZMQ messages using a PUB socket.

)"">;

public:
    gr::PortIn<Pdu<T>> in;
    std::string endpoint = "tcp://*:5555";
    zmq::context_t _context;
    zmq::socket_t _socket = { _context, zmq::socket_type::pub };

    void start() { _socket.bind(endpoint); }

    void processOne(const Pdu<T>& a)
    {
        const size_t size = a.data.size() * sizeof(T);
#ifdef TRACE
        fmt::println("{} sending ZMQ message (length = {})", this->name, size);
#endif
        zmq::message_t zmsg(size);
        memcpy(zmsg.data(), a.data.data(), size);
        _socket.send(zmsg, zmq::send_flags::none);
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::ZmqPduPubSink, in, endpoint);

#endif // _GR4_PACKET_MODEM_ZMQ_PDU_PUB_SINK
