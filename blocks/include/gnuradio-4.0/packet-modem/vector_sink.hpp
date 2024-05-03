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
@brief Vector Sink. Writes input samples into a vector.

This block writes all its input samples into a std::vector, which can be
retrieved after the flowgraph has stopped.

The `reserve_items` parameter indicates the initial number of items to reserve
in the vector. The vector capacity will grow as needed if this number of items
is exceeded.

)"">;

private:
    std::vector<T> d_vector;

public:
    gr::PortIn<T> in;

    VectorSink(size_t reserve_items = 1024) { d_vector.reserve(reserve_items); }

    [[nodiscard]] constexpr auto processOne(T a) noexcept { d_vector.push_back(a); }

    // Returns a copy of the vector stored in the block
    std::vector<T> data() const noexcept { return d_vector; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::VectorSink, in);

#endif // _GR4_PACKET_MODEM_VECTOR_SINK
