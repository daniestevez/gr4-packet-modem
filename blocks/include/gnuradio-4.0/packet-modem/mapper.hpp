#ifndef _GR4_PACKET_MODEM_MAPPER
#define _GR4_PACKET_MODEM_MAPPER

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <vector>

namespace gr::packet_modem {

template <typename TIn, typename TOut>
class Mapper : public gr::Block<Mapper<TIn, TOut>>
{
public:
    using Description = Doc<R""(
@brief Mapper. Maps input elements according to a map defined by a vector.

This block is constructed using a vector `map` whose size must be a power of 2,
say `2^k`. For each input item, the block generates an output item by looking at
the k LSBs of the input item and indexing the `map` using these k LSBs. The
result is the output item. The data in the bits above the k LSBs in the input
items is ignored.

The block can be used for instance to implement a constellation modulator by
mapping nibbles into constellation symbols.

)"">;

private:
    const std::vector<TOut> d_map;
    const size_t d_mask;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;

    Mapper(const std::vector<TOut>& map) : d_map(map), d_mask(map.size() - 1)
    {
        if (!std::has_single_bit(map.size())) {
            throw std::invalid_argument(fmt::format(
                "{} the map size must be a power of 2 (got {})", this->name, map.size()));
        }
    }

    [[nodiscard]] constexpr auto processOne(TIn a) const noexcept
    {
        return d_map[static_cast<size_t>(a) & d_mask];
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Mapper, in, out);

#endif // _GR4_PACKET_MODEM_MAPPER
