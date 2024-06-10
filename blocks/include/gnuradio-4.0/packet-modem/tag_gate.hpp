#ifndef _GR4_PACKET_MODEM_TAG_GATE
#define _GR4_PACKET_MODEM_TAG_GATE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T>
class TagGate : public gr::Block<TagGate<T>>
{
public:
    using Description = Doc<R""(
@brief Tag Gate. Blocks tag propagation through the block.

)"">;

    constexpr static gr::TagPropagationPolicy tag_policy =
        gr::TagPropagationPolicy::TPP_DONT;

public:
    gr::PortIn<T> in;
    gr::PortOut<T> out;

    [[nodiscard]] constexpr T processOne(T a) noexcept { return a; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::TagGate, in, out);

#endif // _GR4_PACKET_MODEM_TAG_GATE
