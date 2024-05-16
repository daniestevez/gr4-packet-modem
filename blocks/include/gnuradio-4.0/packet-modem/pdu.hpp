#ifndef _GR4_PACKET_MODEM_PDU
#define _GR4_PACKET_MODEM_PDU

#include <gnuradio-4.0/Tag.hpp>

namespace gr::packet_modem {

// PDUs are used to collect packets in a stream, together with their tags
// into a `std::vector`. The `data` member contains the items of the packet,
// while the `tags` member contains all the tags attached to items of the packet.
// The offset of each tag corresponds to the index within `data` of the element
// to which the tag is attached.
template <typename T>
struct Pdu {
    using value_type = T;

    std::vector<T> data{};
    std::vector<gr::Tag> tags{};
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PDU
