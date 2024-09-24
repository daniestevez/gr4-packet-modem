#ifndef _GR4_PACKET_MODEM_ADD
#define _GR4_PACKET_MODEM_ADD

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>

namespace gr::packet_modem {

template <typename T>
class Add : public gr::Block<Add<T>>
{
public:
    using Description = Doc<R""(
@brief Add. Adds two inputs.

)"">;

    gr::PortIn<T> in0;
    gr::PortIn<T> in1;
    gr::PortOut<T> out;

    [[nodiscard]] constexpr T processOne(T a, T b) const noexcept { return a + b; }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::Add, in0, in1, out);

#endif // _GR4_PACKET_MODEM_ADD
