#ifndef _GR4_PACKET_MODEM_MESSAGE_DEBUG_STREAM
#define _GR4_PACKET_MODEM_MESSAGE_DEBUG_STREAM

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <iterator>
#include <vector>

namespace gr::packet_modem {

class MessageDebugStream : public gr::Block<MessageDebugStream>
{
public:
    using Description = Doc<R""(
@brief Message Debug (Stream Input). Prints received messages.

Messages received in the `print` port are printed. This is like Message Debug
but with a stream input.

)"">;

public:
    gr::PortIn<gr::Message> print;

    void processOne(const gr::Message& message)
    {
        const auto& data = message.data;
        if (data.has_value()) {
            fmt::println("[MessageDebugStream] {}", data.value());
        } else {
            fmt::println("[MessageDebugStream] ERROR: {}", data.error());
        }
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::MessageDebugStream, print);

#endif // _GR4_PACKET_MODEM_MESSAGE_DEBUG_STREAM
