#ifndef _GR4_PACKET_MODEM_RANDOM_SOURCE
#define _GR4_PACKET_MODEM_RANDOM_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <random>

namespace gr::packet_modem {

template <typename T>
class RandomSource : public gr::Block<RandomSource<T>>
{
public:
    using Description = Doc<R""(
@brief Random Source.

This block is a wrapper over the `VectorSource` block. It generates a random
vector of `num_items` elements in the range `[minimum, maximum]` (NOTE: unlike
the GR3 Random Source block, which generates elements in the range `[minimum,
maximum)`), and constructs a `VectorSource` using that vector. The
`VectorSource` block is set to repeat according to the value of the `repeat`
parameter.

)"">;

public:
    VectorSource<T> _vector_source;

private:
    std::vector<T> random_vector(T min, T max, size_t size)
    {
        std::random_device r;
        std::default_random_engine e(r());
        std::uniform_int_distribution<T> dist(min, max);
        std::vector<T> data;
        data.reserve(size);
        for (size_t j = 0; j < size; ++j) {
            data.push_back(dist(e));
        }
        return data;
    }

public:
    gr::PortOut<T> out;
    T minimum;
    T maximum;
    size_t num_items = 1;
    bool repeat = false;

    void start()
    {
        _vector_source.data = random_vector(minimum, maximum, num_items);
        _vector_source.repeat = repeat;
        _vector_source.start();
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
        return _vector_source.processBulk(outSpan);
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(
    gr::packet_modem::RandomSource, out, minimum, maximum, num_items, repeat);

#endif // _GR4_PACKET_MODEM_RANDOM_SOURCE
