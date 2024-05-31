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

public:
    std::vector<T> _vector;
    std::vector<gr::Tag> _tags;

public:
    gr::PortIn<T> in;
    size_t reserve_items = 1024;

    void start() { _vector.reserve(reserve_items); }

    gr::work::Status processBulk(gr::ConsumableSpan auto& inSpan)
    {
#ifdef TRACE
        fmt::println("{}::processBulk(inSpan.size() = {})", this->name, inSpan.size());
#endif
        if (this->input_tags_present()) {
#ifdef TRACE
            fmt::println("{} received tag ({}, {}) at index = {}",
                         this->name,
                         this->mergedInputTag().index,
                         this->mergedInputTag().map,
                         _vector.size());
#endif
            _tags.emplace_back(_vector.size(), this->mergedInputTag().map);
        }

        _vector.append_range(inSpan);
        return gr::work::Status::OK;
    }

    // Returns a copy of the vector stored in the block
    std::vector<T> data() const noexcept { return _vector; }

    // Returns a copy of the tags stored in the block
    std::vector<gr::Tag> tags() const noexcept { return _tags; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::VectorSink, in, reserve_items);

#endif // _GR4_PACKET_MODEM_VECTOR_SINK
