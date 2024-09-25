#include <gnuradio-4.0/packet-modem/payload_metadata_insert.hpp>

#include "register_helpers.hpp"

void register_payload_metadata_insert()
{
    using namespace gr::packet_modem;
    // FIXME:
    //
    // This block cannot be registered for all scalar types because for some
    // weird reason gr4 tries to simdize its message port when registering it
    // for some types such as float.
    //
    // /gr4-packet-modem/gnuradio4/meta/include/gnuradio-4.0/meta/utils.hpp:257:19:
    // error: static assertion failed due to requirement '[]<std::size_t
    // ...Is>(integer_sequence<unsigned long, _Ip...>) {
    //
    //    return ((simdize_size<gr::Message>::value == simdize_size<typename
    //    tuple_element<Is, tuple<Message, Message, simd<float,
    //    fixed_size<4>>>>::type>::value) && ...);
    //
    // register_all_scalar_types<PayloadMetadataInsert>();

    gr::registerBlock<PayloadMetadataInsert, std::complex<float>>(
        gr::globalBlockRegistry());
}
