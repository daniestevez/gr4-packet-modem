#ifndef _GR4_PACKET_MODEM_PACKET_TO_STREAM
#define _GR4_PACKET_MODEM_PACKET_TO_STREAM

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class PacketToStream : public gr::Block<PacketToStream<T>>
{
public:
    using Description = Doc<R""(
@brief Packet to Stream. Inserts zeros between packets if downstream requires
data but there is no input available.

This block is used to convert a packet-based transmission, where there can pass
a significant time between adjacent packets, into a stream-based transmission,
such as what is required to continuosly feed a DAC at a constant sample rate.

The block works by copying packets from the input to the output. The input
packets are delimited by a packet-length tag in their first item which indicates
the length of the packet. Packets need to be present back-to-back in the
input. The block throws an exception if a packet-length tag is not present N
items after a packet-length tag, where N is the value of that tag. The key used
for the packet-length tag is indicated by the `packet_len_tag_key` constructor
parameter.

If the block `processBulk()` function is called because downstream needs samples
but there is not a new packet present at the input of this block, the block will
fill the output span with zeros and return. In this way, a discontinuous
packet-stream is converted into a continuous stream by inserting zeros between
packets as needed. If the block is in the process of copying an input packet to
the output when `processBulk()` is called, then it requires input to be present
to produce output, in order to finish the packet, instead of inserting zero
mid-packet.

)"">;

private:
    const std::string d_packet_len_tag_key;
    size_t d_remaining;

public:
    // The input port is declared as async to signal the runtime that we do not
    // need an input on this port to produce an output.
    gr::PortIn<T, gr::Async> in;
    gr::PortOut<T> out;

    PacketToStream(const std::string& packet_len_tag_key = "packet_len")
        : d_packet_len_tag_key(packet_len_tag_key), d_remaining(0)
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {}), "
                     "d_remaining = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     d_remaining);
#endif
        if (d_remaining == 0 && inSpan.size() == 0) {
            // We are not mid-packet and there is no input available. Fill the
            // output with zeros and return.
            std::ranges::fill(outSpan, T{ 0 });
            std::ignore = inSpan.consume(0);
            outSpan.publish(outSpan.size());
            return gr::work::Status::OK;
        }

        if (d_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            static constexpr auto not_found =
                "[PacketToStream] expected packet-length tag not found";
            if (!this->input_tags_present()) {
                throw std::runtime_error(not_found);
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(d_packet_len_tag_key)) {
                throw std::runtime_error(not_found);
            }
            d_remaining = pmtv::cast<uint64_t>(tag.map[d_packet_len_tag_key]);
        }

        const auto to_publish = std::min({ inSpan.size(), outSpan.size(), d_remaining });
        std::ranges::copy_n(
            inSpan.begin(), static_cast<ssize_t>(to_publish), outSpan.begin());
        d_remaining -= to_publish;

        // At this point we return instead of trying to fill the rest of the
        // output with zeros. There might be a packet available on the input but
        // not present in inSpan because the scheduler calls processBulk() with
        // one packet at a time, since tags are always aligned to the first item
        // of inSpan.

        std::ignore = inSpan.consume(to_publish);
        outSpan.publish(to_publish);

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketToStream, in, out);

#endif // _GR4_PACKET_MODEM_PACKET_TO_STREAM
