#ifndef _GR4_PACKET_MODEM_VECTOR_SINK
#define _GR4_PACKET_MODEM_VECTOR_SINK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class VectorSink : public gr::Block<VectorSink<T>>
{
public:
    using Description = Doc<R""(
@brief Vector Sink. Writes input items and tags into vectors.

This block writes all its input items into a `std::vector`, and all its input
tags into another `std::vector`. These vectors can be retrieved when the
flowgraph has stopped.

The `reserve_items` parameter indicates the initial number of items to reserve
in the item vector. The vector capacity will grow as needed if this number of
items is exceeded. The tag vector is constructed for an initial capacity of
zero, and grows as necessary.

)"">;

private:
    std::vector<T> d_vector;
    std::vector<gr::Tag> d_tags;

public:
    gr::PortIn<T> in;

    VectorSink(size_t reserve_items = 1024) { d_vector.reserve(reserve_items); }

    gr::work::Status processBulk(gr::ConsumableSpan auto& inSpan)
    {
        if (this->input_tags_present()) {
            d_tags.emplace_back(d_vector.size(), this->mergedInputTag().map);
        }

        d_vector.append_range(inSpan);
        std::ignore = inSpan.consume(inSpan.size());

        return gr::work::Status::OK;
    }

    // Returns a copy of the vector stored in the block
    std::vector<T> data() const noexcept { return d_vector; }

    // Returns a copy of the tags stored in the block
    std::vector<gr::Tag> tags() const noexcept { return d_tags; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::VectorSink, in);

#endif // _GR4_PACKET_MODEM_VECTOR_SINK
