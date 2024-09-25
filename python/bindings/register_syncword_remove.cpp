#include <gnuradio-4.0/packet-modem/syncword_remove.hpp>

#include "register_helpers.hpp"

void register_syncword_remove()
{
    using namespace gr::packet_modem;
    register_all_scalar_types<SyncwordRemove>();
}
