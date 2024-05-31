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

public:
    ssize_t _position;

private:
    void check_vector()
    {
        if (data.empty()) {
            throw gr::exception("data is empty");
        }
        for (const auto& tag : tags) {
            if (tag.index < 0 || static_cast<size_t>(tag.index) >= data.size()) {
                throw gr::exception(
                    fmt::format("tag {} has invalid index {}", tag.map, tag.index));
            }
        }
    }

public:
    gr::PortOut<T> out;
    std::vector<T> data;
    bool repeat = false;
    // this cannot be updated through settings because gr::Tag cannot be
    // converted to pmtv
    std::vector<gr::Tag> tags;

    // void settingsChanged(const gr::property_map& /* old_settings */,
    //                      const gr::property_map& /* new_settings */)
    // {
    // }

    void start()
    {
        check_vector();
        _position = 0;
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(outSpan.size() = {})", this->name, outSpan.size());
#endif
        // copy what remains of the current "loop" of the vector
        const auto n = std::min(std::ssize(data) - _position, std::ssize(outSpan));
        auto output = outSpan.begin();
        std::ranges::copy(data | std::views::drop(_position) | std::views::take(n),
                          output);
        for (const auto& tag : tags) {
            if (tag.index >= _position && tag.index - _position < n) {
                out.publishTag(tag.map, tag.index - _position);
            }
        }
        output += n;
        _position += n;

        // exit here if we don't repeat
        if (!repeat) {
            outSpan.publish(static_cast<size_t>(n));
#ifdef TRACE
            if (_position == std::ssize(data)) {
                fmt::println("{}::processBulk returning DONE", this->name);
            } else {
                fmt::println("{}::processBulk returning OK", this->name);
            }
#endif
            return _position == std::ssize(data) ? gr::work::Status::DONE
                                                 : gr::work::Status::OK;
        }

        if (_position != std::ssize(data)) {
            // there is no more output
            outSpan.publish(static_cast<size_t>(n));
#ifdef TRACE
            fmt::println("{}::processBulk returning OK", this->name);
#endif
            return gr::work::Status::OK;
        }

        // fill the rest of the output with full loops of the vector
        while (outSpan.end() - output >= std::ssize(data)) {
            std::ranges::copy(data, output);
            for (const auto& tag : tags) {
                out.publishTag(tag.map, output - outSpan.begin() + tag.index);
            }
            output += std::ssize(data);
        }
        // fill the end of the output with a partial loop
        _position = outSpan.end() - output;
        std::ranges::copy(data | std::views::take(_position), output);
        for (const auto& tag : tags) {
            if (tag.index < _position) {
                out.publishTag(tag.map, output - outSpan.begin() + tag.index);
            }
        }
        output += _position;

        outSpan.publish(outSpan.size());
#ifdef TRACE
        fmt::println("{}::processBulk returning OK", this->name);
#endif
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

// currently `data` is disabled from reflection because VectorSource is often
// used with Pdu<T>, which is not convertible to PMT because gr::Tag is not
// convertible to PMT
ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::VectorSource, out, /*data,*/ repeat);

#endif // _GR4_PACKET_MODEM_VECTOR_SOURCE
