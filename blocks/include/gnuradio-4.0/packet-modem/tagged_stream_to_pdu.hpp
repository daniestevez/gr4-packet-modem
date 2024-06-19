#ifndef _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU
#define _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class TaggedStreamToPdu : public gr::Block<TaggedStreamToPdu<T>>
{
public:
    using Description = Doc<R""(
@brief Tagged Stream to PDU. Converts a stream of packets delimited by packet length tags
into a stream of PDUs.

The input of this block is a stream of items of type `T` forming packets
delimited by a packet-length tag that is present in the first item of the packet
and whose value indicates the length of the packet. It converts the packets in
the input into a stream of `Pdu` objects, where each packet is stored as the
`data` vector in the `Pdu`, and the tags previously attached to items of the
packet are stored as the `tags` vector in the `Pdu`.

)"">;

public:
    uint64_t _remaining;
    uint64_t _index;
    Pdu<T> _pdu;

public:
    gr::PortIn<T> in;
    gr::PortOut<Pdu<T> /*, gr::RequiredSamples<1U, 1U, false>*/> out;
    // This causes compile errors:
    // gr::PortOut<Pdu<T>, gr::RequiredSamples<1U, 1U, true>> out;
    std::string packet_len_tag_key = "packet_len";

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void start() { _remaining = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = "
                     "{}), _remaining = {}, _pdu.data.size() = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _remaining,
                     _pdu.data.size());
        if (this->input_tags_present()) {
            const auto tag = this->mergedInputTag();
            fmt::println("{} tags: index = {}, map = {}", this->name, tag.index, tag.map);
        }
#endif
        if (inSpan.size() == 0) {
            if (!inSpan.consume(0)) {
                throw gr::exception("consume failed");
            }
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }
        if (outSpan.size() == 0) {
            // we always require space in the output, just in case we need to
            // publish an output PDU
            if (!inSpan.consume(0)) {
                throw gr::exception("consume failed");
            }
            outSpan.publish(0);
            return gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS;
        }

        if (_remaining == 0) {
            // Fetch the packet length tag to determine the length of the packet.
            auto not_found_error = [this]() {
                this->emitErrorMessage(
                    fmt::format("{}::processBulk", this->name),
                    fmt::format("{} expected packet-length tag not found", this->name));
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
            _remaining = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
            if (_remaining == 0) {
                this->emitErrorMessage(
                    fmt::format("{}::processBulk", this->name),
                    fmt::format("{} received packet-length equal to zero", this->name));
                this->requestStop();
                return gr::work::Status::ERROR;
            }
            _pdu.data.clear();
            _pdu.data.reserve(_remaining);
            _pdu.tags.clear();
            _index = 0;
        }

        if (this->input_tags_present()) {
            auto tag = this->mergedInputTag();
            tag.index = static_cast<ssize_t>(_index);
            // remove packet_len tag
            if (_index == 0) {
                tag.map.erase(packet_len_tag_key);
            }
            // the map can be empty if it only contained the packet_len tag
            if (!tag.map.empty()) {
                _pdu.tags.push_back(tag);
            }
        }

        const auto to_consume = std::min(_remaining, inSpan.size());
        std::ranges::copy(inSpan | std::views::take(to_consume),
                          std::back_inserter(_pdu.data));
        if (!inSpan.consume(to_consume)) {
            throw gr::exception("consume failed");
        }
        _remaining -= to_consume;
        _index += to_consume;

        if (_remaining == 0) {
#ifdef TRACE
            if (!_pdu.tags.empty()) {
                fmt::println("{} publishing PDU with tags:", this->name);
                for (const auto& tag : _pdu.tags) {
                    fmt::println("index = {}, map = {}", tag.index, tag.map);
                }
            }
#endif
            outSpan[0] = std::move(_pdu);
            _pdu = {};
            outSpan.publish(1);
#ifdef TRACE
            fmt::println("{} consume = {}, publish = 1", this->name, to_consume);
#endif
        } else {
            outSpan.publish(0);
            // Clear input tags. This is needed because the block doesn't
            // publish anything, so the input tags don't get cleared by the
            // runtime.
            this->_mergedInputTag.map.clear();
#ifdef TRACE
            fmt::println("{} consume = {}, publish = 0, _remaining = {}, "
                         "_pdu.data.size() = {}",
                         this->name,
                         to_consume,
                         _remaining,
                         _pdu.data.size());
#endif
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::TaggedStreamToPdu,
                               in,
                               out,
                               packet_len_tag_key);

#endif // _GR4_PACKET_MODEM_TAGGED_STREAM_TO_PDU
