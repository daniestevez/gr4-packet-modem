#ifndef _GR4_PACKET_MODEM_NULL_SINK
#define _GR4_PACKET_MODEM_NULL_SINK

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T>
class NullSink : public gr::Block<NullSink<T>>
{
public:
    using Description = Doc<R""(
@brief Null Source. Sinks and discards input indefinitely.

)"">;

public:
    gr::PortIn<T> in;

    [[nodiscard]] constexpr auto processOne([[maybe_unused]] T _a) const noexcept
    {
        return;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::NullSink, in);

#endif // _GR4_PACKET_MODEM_NULL_SINK
