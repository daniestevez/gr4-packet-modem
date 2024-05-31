#ifndef _GR4_PACKET_MODEM_BURST_SHAPER
#define _GR4_PACKET_MODEM_BURST_SHAPER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>

namespace gr::packet_modem {

template <typename TIn, typename TOut = TIn, typename TShape = TIn>
class BurstShaper : public gr::Block<BurstShaper<TIn, TOut, TShape>>
{
public:
    using Description = Doc<R""(
@brief Burst Shaper. Applies amplitude shaping to the first and last items of
each packet.

The input and output of this block are streams of items forming packets
delimited by a packet-length tag that is present in the first item of the packet
and whose value indicates the length of the packet.

The block is given a `leading_shape` vector and a `trailing_shape` vector. It
multiplies the first `leading_shape.size()` items in each packet by the elements
of `leading_shape` and the last `trailing_shape.size()` items in each packet by
the elements of `trailing_shape`.

The type of the input items, output items and shape vector elements is given by
the `TIn`, `TOut` and `TShape` template parameters respectively.

)"">;

public:
    uint64_t _remaining;
    uint64_t _packet_len;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    std::vector<TShape> leading_shape;
    std::vector<TShape> trailing_shape;
    std::string packet_len_tag_key = "packet_len";

    void start() { _remaining = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size = {})",
                     this->name,
                     inSpan.size(),
                     outSpan.size());
#endif
        if (inSpan.size() == 0) {
            std::ignore = inSpan.consume(0);
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        if (_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            auto not_found_error = [this]() {
                this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                       "expected packet-length tag not found");
                this->requestStop();
                return gr::work::Status::ERROR;
            };
            if (!this->input_tags_present()) {
                return not_found_error();
            }
            auto tag = this->mergedInputTag();
            if (!tag.map.contains(packet_len_tag_key)) {
                return not_found_error();
            }
            const auto packet_len = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
            if (packet_len == 0) {
                this->emitErrorMessage(fmt::format("{}::processBulk", this->name),
                                       "received packet-length equal to zero");
                this->requestStop();
                return gr::work::Status::ERROR;
            }
            _remaining = packet_len;
            _packet_len = packet_len;
        }

        auto in_item = inSpan.begin();
        auto out_item = outSpan.begin();

        const auto position = _packet_len - _remaining;
        size_t n = 0;
        if (position < leading_shape.size()) {
            // multiply samples by leading shape
            n = std::min(
                { leading_shape.size() - position, inSpan.size(), outSpan.size() });
            for (size_t j = position; j < position + n; ++j) {
                *out_item++ = *in_item++ * leading_shape[j];
            }
            _remaining -= n;
        }
        if (_remaining > trailing_shape.size()) {
            // copy samples without shaping
            const size_t m = std::min({ _remaining - trailing_shape.size(),
                                        inSpan.size() - n,
                                        outSpan.size() - n });
            std::copy_n(in_item, m, out_item);
            in_item += static_cast<ssize_t>(m);
            out_item += static_cast<ssize_t>(m);
            n += m;
            _remaining -= m;
        }
        // multiply samples by trailing edge
        const size_t m = std::min({ _remaining, inSpan.size() - n, outSpan.size() - n });
        if (m > 0) {
            const auto start = trailing_shape.size() - _remaining;
            const auto end = start + m;
            for (size_t j = start; j < end; ++j) {
                *out_item++ = *in_item++ * trailing_shape[j];
            }
            n += m;
            _remaining -= m;
        }

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

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::BurstShaper,
                               in,
                               out,
                               leading_shape,
                               trailing_shape,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_BURST_SHAPER
