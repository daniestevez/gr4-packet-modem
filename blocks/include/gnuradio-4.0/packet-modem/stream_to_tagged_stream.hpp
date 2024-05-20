#ifndef _GR4_PACKET_MODEM_STREAM_TO_TAGGED_STREAM
#define _GR4_PACKET_MODEM_STREAM_TO_TAGGED_STREAM

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <cstddef>
#include <ranges>

namespace gr::packet_modem {

template <typename T>
class StreamToTaggedStream : public gr::Block<StreamToTaggedStream<T>>
{
public:
    using Description = Doc<R""(
@brief Stream to Tagged Stream. Converts a stream into a stream of packets
delimited by packet-length tags.

This blocks inserts packet-length tags every `packet_length` items in order to
convert a stream into a stream of packets delimited by packet-length tags. The
value of these tags is `packet_length`.

)"">;

private:
    const uint64_t d_packet_length;
    const std::string d_packet_len_tag_key;
    uint64_t d_count = 0;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    StreamToTaggedStream(uint64_t packet_length,
                         const std::string& packet_len_tag_key = "packet_len")
        : d_packet_length(packet_length), d_packet_len_tag_key(packet_len_tag_key)
    {
    }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println(
            "StreamToTaggedStream::processBulk(inSpan.size() = {}, outSpan.size = {}), "
            "d_count = {}",
            inSpan.size(),
            outSpan.size(),
            d_count);
#endif
        const auto n = std::min(inSpan.size(), outSpan.size());
        std::ranges::copy_n(inSpan.begin(), static_cast<ssize_t>(n), outSpan.begin());
        for (uint64_t index = d_count == 0 ? 0 : d_packet_length - d_count; index < n;
             index += d_packet_length) {
#ifdef TRACE
            fmt::println("StreamToTaggedStream publishTag(index = {})", index);
#endif
            out.publishTag({ { d_packet_len_tag_key, d_packet_length } },
                           static_cast<ssize_t>(index));
        }
        d_count = (d_count + n) % d_packet_length;
        std::ignore = inSpan.consume(n);
        outSpan.publish(n);
        if (n == 0) {
            return inSpan.size() == 0 ? gr::work::Status::INSUFFICIENT_INPUT_ITEMS
                                      : gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::StreamToTaggedStream, in, out);

#endif // _GR4_PACKET_MODEM_STREAM_TO_TAGGED_STREAM
