#ifndef _GR4_PACKET_MODEM_PACKET_MUX
#define _GR4_PACKET_MODEM_PACKET_MUX

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <numeric>
#include <ranges>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class PacketMux : public gr::Block<PacketMux<T>>
{
public:
    using Description = Doc<R""(
@brief Packet Mux. Concatenates one packet from each input to form an output packet.

This block receives a stream of packets delimited with packet-length tags in
each input port. It concatenatates a packet from each input in order to form an
output packet, whose length is the sum of the lengths of these packets. The
block waits until there is input available in each input port in order to
produce a new output packet, since the packet-length tags on each input port are
required to calculate the output packet-length tag.

)"">;

public:
    std::vector<size_t> _packet_lens;
    size_t _current_input;
    size_t _remaining;

public:
    std::vector<gr::PortIn<T, gr::Async>> in;
    gr::PortOut<T, gr::Async> out;
    size_t num_inputs = 0;
    std::string packet_len_tag = "packet_len";

    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (num_inputs == 0) {
            throw gr::exception("num_inputs cannot be zero");
        }
        in.resize(num_inputs);
    }

    void start() { _packet_lens.clear(); }

    // function to get the tags at the current read position from a port, since
    // there doesn't seem to be an appropriate one in Port.hpp
    static inline gr::property_map _get_tags(const gr::PortIn<T, gr::Async>& inPort)
    {
        gr::property_map map{};
        const auto position = inPort.streamReader().position();
        for (const auto& tag : inPort.tags) {
            if (tag.index == position) {
                for (const auto& [key, value] : tag.map) {
                    map.insert_or_assign(key, value);
                }
            }
        }
        return map;
    }

    // similar to Port::nSamplesUntilNextTag, but does not call tagData.consume(0UZ)
    static inline constexpr std::optional<std::size_t>
    _samples_to_next_tag(const gr::PortIn<T, gr::Async>& inPort, ssize_t offset)
    {
        const gr::ConsumableSpan auto tag_data = inPort.tagReader().get();
        if (tag_data.empty()) {
            return std::nullopt;
        }
        const auto read_position = inPort.streamReader().position();
        const auto match = std::ranges::find_if(tag_data, [&](const auto& tag) {
            return tag.index >= read_position + offset;
        });
        if (match != tag_data.end()) {
            return static_cast<std::size_t>(match->index - read_position);
        } else {
            return std::nullopt;
        }
    }

    template <gr::ConsumableSpan TInput>
    gr::work::Status processBulk(const std::span<TInput>& inSpans,
                                 [[maybe_unused]] gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::print("{}::processBulk(outSpan.size() = {}) ", this->name, outSpan.size());
        for (size_t j = 0; j < inSpans.size(); ++j) {
            fmt::print("inSpans[{}].size() = {} ", j, inSpans[j].size());
        }
        fmt::print("\n");
#endif

        if (_packet_lens.empty()) {
            // try to fetch packet lengths tags for each input
            const auto min_in_size =
                std::ranges::min(inSpans | std::views::transform([](const auto& span) {
                                     return span.size();
                                 }));
            if (min_in_size == 0) {
                for (auto& inSpan : inSpans) {
                    if (!inSpan.consume(0)) {
                        throw gr::exception("consume failed");
                    }
                }
                outSpan.publish(0);
                return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
            }
            for (auto& inPort : in) {
                const auto map = _get_tags(inPort);
#ifdef TRACE
                fmt::println("map = {}", map);
#endif
                if (!map.contains(packet_len_tag)) {
                    throw gr::exception("expected packet_len tag key not found");
                }
                _packet_lens.push_back(pmtv::cast<uint64_t>(map.at(packet_len_tag)));
            }
            _current_input = 0;
            _remaining = _packet_lens[0];
            const uint64_t total_len = static_cast<uint64_t>(
                std::accumulate(_packet_lens.cbegin(), _packet_lens.cend(), 0));
            out.publishTag({ { packet_len_tag, total_len } }, 0);
        }

        size_t published = 0;

        for (size_t j = 0; j < _current_input; ++j) {
            if (!inSpans[j].consume(0)) {
                throw gr::exception("consume failed");
            }
        }

        while (published < outSpan.size()) {
            // Block doesn't chunk Async inputs based on tags, so we chunk it
            // here and avoid consuming past the next tag
            const size_t samples_to_tag =
                _samples_to_next_tag(in[_current_input], 1)
                    .value_or(std::numeric_limits<std::size_t>::max());
            const size_t n = std::min({ inSpans[_current_input].size(),
                                        samples_to_tag,
                                        _remaining,
                                        outSpan.size() - published });
#ifdef TRACE
            fmt::println("{} _current_input = {}, samples_to_tag = {}, n = {}",
                         this->name,
                         _current_input,
                         samples_to_tag,
                         n);
#endif
            std::copy_n(inSpans[_current_input].begin(),
                        n,
                        outSpan.begin() + static_cast<ssize_t>(published));
            auto map = _get_tags(in[_current_input]);
            map.erase(packet_len_tag);
            if (!map.empty()) {
                out.publishTag(map, static_cast<ssize_t>(published));
            }
            published += n;
            if (!inSpans[_current_input].consume(n)) {
                throw gr::exception("consume failed");
            }
            _remaining -= n;
            if (_remaining == 0) {
                ++_current_input;
                if (_current_input == in.size()) {
                    _packet_lens.clear();
                    break;
                }
                _remaining = _packet_lens[_current_input];
            } else {
                for (size_t j = _current_input + 1; j < in.size(); ++j) {
                    if (!inSpans[j].consume(0)) {
                        throw gr::exception("consume failed");
                    }
                }
                break;
            }
        }

#ifdef TRACE
        fmt::println("{} outSpan.publish({})", this->name, published);
#endif
        outSpan.publish(published);

        return gr::work::Status::OK;
    }
};

template <typename T>
class PacketMux<Pdu<T>> : public gr::Block<PacketMux<Pdu<T>>>
{
public:
    using Description = Doc<R""(
@brief Packet Mux (PDU specialization). Concatenates one packet from each input
to form an output packet.

This block is a PDU specialization of the Packet Mux block. It receives a PDU on
each input port and concatenates them to produce a PDU in the output port. The
block waits until there is input available in each input port in order to
produce a new output PDU.

)"">;

public:
    std::vector<gr::PortIn<Pdu<T>>> in;
    gr::PortOut<Pdu<T>> out;
    size_t num_inputs = 0;
    std::string packet_len_tag = "packet_len";

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (num_inputs == 0) {
            throw gr::exception("num_inputs cannot be zero");
        }
        in.resize(num_inputs);
    }

    template <gr::ConsumableSpan TInput>
    gr::work::Status processBulk(const std::span<TInput>& inSpans,
                                 gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::print("{}::processBulk(outSpan.size() = {}) ", this->name, outSpan.size());
        for (size_t j = 0; j < inSpans.size(); ++j) {
            fmt::print("inSpans[{}].size() = {} ", j, inSpans[j].size());
        }
        fmt::print("\n");
#endif

#ifndef NDEBUG
        for (const auto& inSpan : inSpans) {
            assert(inSpan.size() == outSpan.size());
        }
#endif

        const auto n = outSpan.size();
        assert(n > 0);

        for (const size_t j : std::views::iota(0UZ, n)) {
            outSpan[j].data = inSpans[0][j].data;
            outSpan[j].tags = inSpans[0][j].tags;
            for (const size_t k : std::views::iota(1UZ, inSpans.size())) {
                const ssize_t tag_offset = std::ssize(outSpan[j].data);
                outSpan[j].data.insert(outSpan[j].data.end(),
                                       inSpans[k][j].data.cbegin(),
                                       inSpans[k][j].data.cend());
                for (const auto& tag : inSpans[k][j].tags) {
                    outSpan[j].tags.emplace_back(tag_offset + tag.index, tag.map);
                }
            }
        }

        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketMux, in, out, num_inputs);

#endif // _GR4_PACKET_MODEM_PACKET_MUX
