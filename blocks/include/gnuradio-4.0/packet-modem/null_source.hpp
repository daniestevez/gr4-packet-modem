#ifndef _GR4_PACKET_MODEM_NULL_SOURCE
#define _GR4_PACKET_MODEM_NULL_SOURCE

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T>
class NullSource : public gr::Block<NullSource<T>>
{
public:
    using Description = Doc<R""(
@brief Null Source. Produces zero indefinitely.

)"">;

    gr::PortOut<T> out;

    [[nodiscard]] constexpr auto processOne() const noexcept { return T{ 0 }; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::NullSource, out);

#endif // _GR4_PACKET_MODEM_NULL_SOURCE
