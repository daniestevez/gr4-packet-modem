#ifndef _GR4_PACKET_MODEM_PACKET_TYPE_FILTER
#define _GR4_PACKET_MODEM_PACKET_TYPE_FILTER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/packet_type.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>

namespace gr::packet_modem {

template <typename T = uint8_t>
class PacketTypeFilter : public gr::Block<PacketTypeFilter<T>>
{
public:
    using Description = Doc<R""(
@brief Packet Type Filter. Filters packets according to their packet type.

This block only lets through packets of a certain packet type, dropping
everything else.

)"">;

public:
    size_t _remaining;
    bool _valid;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;
    std::string packet_len_tag_key = "packet_len";
    PacketType _packet_type = PacketType::USER_DATA;
    std::string packet_type{ magic_enum::enum_name(_packet_type) };

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        _packet_type =
            magic_enum::enum_cast<PacketType>(packet_type, magic_enum::case_insensitive)
                .value();
    }

    void start() { _remaining = 0; }

    gr::work::Status processBulk(const gr::ConsumableSpan auto& inSpan,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {}, outSpan.size() = {}), "
                     "_remaining = {}, _valid = {}",
                     this->name,
                     inSpan.size(),
                     outSpan.size(),
                     _remaining,
                     _valid);
#endif
        if (_remaining == 0) {
            // Fetch a new packet_len tag
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
            _remaining = pmtv::cast<uint64_t>(tag.map[packet_len_tag_key]);
            const auto packet_type_in =
                magic_enum::enum_cast<PacketType>(
                    pmtv::cast<std::string>(tag.map.at("packet_type")),
                    magic_enum::case_insensitive)
                    .value();
            _valid = packet_type_in == _packet_type;
            if (_valid) {
#ifdef TRACE
                fmt::println("{} publishTag({}, 0)", this->name, tag.map);
#endif
                out.publishTag(tag.map, 0);
            }
        } else if (_valid && this->input_tags_present()) {
#ifdef TRACE
            fmt::println("{} publishTag({}, 0)", this->name, this->mergedInputTag().map);
#endif
            out.publishTag(this->mergedInputTag().map, 0);
        }

        auto to_consume = std::min(_remaining, inSpan.size());
        if (_valid) {
            to_consume = std::min(to_consume, outSpan.size());
            std::ranges::copy_n(
                inSpan.begin(), static_cast<ssize_t>(to_consume), outSpan.begin());
            outSpan.publish(to_consume);
        } else {
            outSpan.publish(0);
        }
        _remaining -= to_consume;
        if (!inSpan.consume(to_consume)) {
            throw gr::exception("consume failed");
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::PacketTypeFilter, in, out, packet_len_tag_key, packet_type);

#endif // _GR4_PACKET_MODEM_PACKET_TYPE_FILTER
