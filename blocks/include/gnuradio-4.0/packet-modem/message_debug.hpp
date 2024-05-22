#ifndef _GR4_PACKET_MODEM_MESSAGE_DEBUG
#define _GR4_PACKET_MODEM_MESSAGE_DEBUG

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <iterator>
#include <vector>

namespace gr::packet_modem {

class MessageDebug : public gr::Block<MessageDebug>
{
public:
    using Description = Doc<R""(
@brief Message Debug. Prints or stores received messages.

Messages received in the `print` port are printed. Messages received in the
`store` port are stored. The stored messages can be retrieved with the
`messages()` method.

)"">;

private:
    std::vector<gr::Message> d_messages;

public:
    gr::MsgPortInNamed<"print"> print;
    gr::MsgPortInNamed<"store"> store;

    void processMessages(gr::MsgPortInNamed<"print">&,
                         std::span<const gr::Message> messages)
    {
        for (const auto& message : messages) {
            const auto& data = message.data;
            if (data.has_value()) {
                fmt::println("[MessageDebug] {}", data.value());
            } else {
                fmt::println("[MessageDebug] ERROR: {}", data.error());
            }
        }
    }

    void processMessages(gr::MsgPortInNamed<"store">&,
                         std::span<const gr::Message> messages)
    {
        std::ranges::copy(messages, std::back_inserter(d_messages));
    }

    std::vector<gr::Message> messages() { return d_messages; }

    // If I don't include this 'fake_in' port and 'processOne' method, I get the
    // following compile errors:
    //
    // no member named 'merged_work_chunk_size' in 'gr::packet_modem::MessageDebug'
    //
    // static assertion failed due to requirement 'gr::meta::always_false<std::tuple<>>':
    // neither processBulk(...) nor processOne(...) implemented
    //
    gr::PortIn<int> fake_in;

    void processOne(int) { return; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION(gr::packet_modem::MessageDebug, fake_in, print, store);

#endif // _GR4_PACKET_MODEM_MESSAGE_DEBUG
