#ifndef _GR4_PACKET_MODEM_TEST_COMPLEX_SETTINGS
#define _GR4_PACKET_MODEM_TEST_COMPLEX_SETTINGS

#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/reflection.hpp>
#include <algorithm>
#include <vector>

namespace gr::packet_modem {

template <typename T>
class TestComplexSettings : public gr::Block<TestComplexSettings<T>>
{
public:
    using Description = Doc<R""(
@brief Block to test std::complex<T> settings

)"">;

public:
    gr::PortOut<T> out;
    std::vector<T> vector;
    T scalar;

    void settingsChanged(const gr::property_map& /* old_settings */,
                         const gr::property_map& /* new_settings */)
    {
        fmt::println("{}::settingsChanged(), vector = {}, scalar = {}",
                     this->name,
                     vector,
                     scalar);
    }

    gr::work::Status processBulk(gr::PublishableSpan auto& outSpan)
    {
        std::fill(outSpan.begin(), outSpan.end(), T{});
        return gr::work::Status::OK;
    }
};

} // namespace gr::packet_modem

ENABLE_REFLECTION_FOR_TEMPLATE(gr::packet_modem::TestComplexSettings,
                               out,
                               vector,
                               scalar);

#endif // _GR4_PACKET_MODEM_TEST_COMPLEX_SETTINGS
