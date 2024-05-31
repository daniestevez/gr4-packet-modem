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

This block has a vector `map` whose size must be a power of 2, say `2^k`. For
each input item, the block generates an output item by looking at the k LSBs of
the input item and indexing the `map` using these k LSBs. The result is the
output item. The data in the bits above the k LSBs in the input items is
ignored.

The block can be used for instance to implement a constellation modulator by
mapping nibbles into constellation symbols.

)"">;

public:
    size_t _mask;

public:
    gr::PortIn<TIn> in;
    gr::PortOut<TOut> out;
    std::vector<TOut> map;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        if (!std::has_single_bit(map.size())) {
            throw gr::exception(
                fmt::format("the map size must be a power of 2 (got {})", map.size()));
        }
        _mask = map.size() - 1;
    }

    [[nodiscard]] constexpr auto processOne(TIn a) const noexcept
    {
        return map[static_cast<size_t>(a) & _mask];
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Mapper, in, out, map);

#endif // _GR4_PACKET_MODEM_MAPPER
