#ifndef _GR4_PACKET_MODEM_VECTOR_SOURCE
#define _GR4_PACKET_MODEM_VECTOR_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <ranges>
#include <stdexcept>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class VectorSource : public gr::Block<VectorSource<T>>
{
public:
    using Description = Doc<R""(
@brief Vector Source. Produces output from a given vector, either once or repeatedly.

This block is given a vector `data`. The output it produces is the samples in
`data`, either once or repeated indefinitely, according to the `repeat`
parameter.

The block can also generate tags, which are given as a vector in the `tags`
parameter. The indices of these tags should all be in the range
`[0, data.size())`. If the block is set to repeat, tags will also be repeated in
the same relative positions of the data.

)"">;

private:
    const std::vector<T> d_vector;
    const bool d_repeat;
    const std::vector<gr::Tag> d_tags;
    ssize_t d_position = 0;

public:
    gr::PortOut<T> out;

    VectorSource(const std::vector<T>& data,
                 bool repeat = false,
                 const std::vector<gr::Tag>& tags = std::vector<gr::Tag>())
        : d_vector(data), d_repeat(repeat), d_tags(tags)
    {
        if (d_vector.empty()) {
            throw std::invalid_argument("[VectorSource] data is empty");
        }
        for (const auto& tag : tags) {
            if (tag.index < 0 || static_cast<size_t>(tag.index) >= data.size()) {
                throw std::invalid_argument(fmt::format(
                    "[VectorSource] tag {} has invalid index {}", tag.map, tag.index));
            }
        }
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::print("VectorSource::processBulk(outSpan.size() = {})\n", outSpan.size());
#endif
        // copy what remains of the current "loop" of the vector
        const auto n = std::min(std::ssize(d_vector) - d_position, std::ssize(outSpan));
        auto output = outSpan.begin();
        std::ranges::copy(d_vector | std::views::drop(d_position) | std::views::take(n),
                          output);
        for (const auto& tag : d_tags) {
            if (tag.index >= d_position && tag.index - d_position < n) {
                out.publishTag(tag.map, tag.index - d_position);
            }
        }
        output += n;
        d_position += n;

        // exit here if we don't repeat
        if (!d_repeat) {
            outSpan.publish(static_cast<size_t>(n));
#ifdef TRACE
            if (d_position == std::ssize(d_vector)) {
                fmt::print("VectorSource::processBulk returning DONE\n");
            } else {
                fmt::print("VectorSource::processBulk returning OK\n");
            }
#endif
            return d_position == std::ssize(d_vector) ? gr::work::Status::DONE
                                                      : gr::work::Status::OK;
        }

        if (d_position != std::ssize(d_vector)) {
            // there is no more output
            outSpan.publish(static_cast<size_t>(n));
            return gr::work::Status::OK;
        }

        // fill the rest of the output with full loops of the vector
        while (outSpan.end() - output >= std::ssize(d_vector)) {
            std::ranges::copy(d_vector, output);
            for (const auto& tag : d_tags) {
                out.publishTag(tag.map, output - outSpan.begin() + tag.index);
            }
            output += std::ssize(d_vector);
        }
        // fill the end of the output with a partial loop
        d_position = outSpan.end() - output;
        std::ranges::copy(d_vector | std::views::take(d_position), output);
        for (const auto& tag : d_tags) {
            if (tag.index < d_position) {
                out.publishTag(tag.map, output - outSpan.begin() + tag.index);
            }
        }
        output += d_position;

        outSpan.publish(outSpan.size());
#ifdef TRACE
        fmt::print("VectorSource::processBulk returning OK\n");
#endif
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::VectorSource, out);

#endif // _GR4_PACKET_MODEM_VECTOR_SOURCE
