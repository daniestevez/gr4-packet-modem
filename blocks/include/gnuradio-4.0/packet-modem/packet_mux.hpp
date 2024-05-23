#ifndef _GR4_PACKET_MODEM_PACKET_MUX
#define _GR4_PACKET_MODEM_PACKET_MUX

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/reflection.hpp>
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

private:
    const std::string d_packet_len_tag;
    std::vector<size_t> d_packet_lens;
    size_t d_current_input;
    size_t d_remaining;

public:
    std::vector<gr::PortIn<T>> in;
    gr::PortOut<T> out;

    constexpr static TagPropagationPolicy tag_policy = TagPropagationPolicy::TPP_CUSTOM;

    PacketMux(size_t num_inputs, const std::string& packet_len_tag = "packet_len")
        : d_packet_len_tag(packet_len_tag), in(num_inputs)
    {
        if (num_inputs == 0) {
            throw std::invalid_argument(
                fmt::format("{} num_inputs cannot be zero", this->name));
        }
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

        if (d_packet_lens.empty()) {
            // try to fetch packet lengths tags for each input
            const auto min_in_size =
                std::ranges::min(inSpans | std::views::transform([](const auto& span) {
                                     return span.size();
                                 }));
            if (min_in_size == 0) {
                return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
            }
            // TODO: this isn't working, because the tags from all the input
            // ports get merged an overwritten
            fmt::println(
                "{}::input_tags_present() = {}", this->name, this->input_tags_present());
            if (this->input_tags_present()) {
                const auto tag = this->mergedInputTag();
                fmt::print(
                    "PacketMux::mergedInputTag() = ({}, {})\n", tag.index, tag.map);
            }
            for (auto& inPort : in) {
                fmt::println("getting tags for input port");
                for (const auto& tag : inPort.getTags(min_in_size)) {
                    fmt::print("tag = ({}, {})\n", tag.index, tag.map);
                }
            }
        }

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

    PacketMux(size_t num_inputs) : in(num_inputs)
    {
        if (num_inputs == 0) {
            throw std::invalid_argument("[PacketMux] num_inputs cannot be zero");
        }
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

        const auto min_in_size =
            std::ranges::min(inSpans | std::views::transform(
                                           [](const auto& span) { return span.size(); }));
        const auto n = std::min(min_in_size, outSpan.size());

        for (size_t j = 0; j < n; ++j) {
            outSpan[j].data = inSpans[0][j].data;
            outSpan[j].tags = inSpans[0][j].tags;
            for (size_t k = 1; k < inSpans.size(); ++k) {
                const ssize_t tag_offset = std::ssize(outSpan[j].data);
                outSpan[j].data.insert(outSpan[j].data.end(),
                                       inSpans[k][j].data.cbegin(),
                                       inSpans[k][j].data.cend());
                for (const auto& tag : inSpans[k][j].tags) {
                    outSpan[j].tags.emplace_back(tag_offset + tag.index, tag.map);
                }
            }
        }

        for (const auto& span : inSpans) {
            std::ignore = span.consume(n);
        }
        outSpan.publish(n);

        if (min_in_size == 0) {
            return gr::work::Status::INSUFFICIENT_INPUT_ITEMS;
        }

        return n == 0 ? gr::work::Status::INSUFFICIENT_OUTPUT_ITEMS
                      : gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::PacketMux, in, out);

#endif // _GR4_PACKET_MODEM_PACKET_MUX
