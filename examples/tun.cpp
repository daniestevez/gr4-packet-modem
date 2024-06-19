#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/tun_sink.hpp>
#include <gnuradio-4.0/packet-modem/tun_source.hpp>
#include <boost/ut.hpp>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun_tx" }, { "netns_name", "gr4_tx" } });
    auto& sink = fg.emplaceBlock<gr::packet_modem::TunSink>(
        { { "tun_name", "gr4_tun_rx" }, { "netns_name", "gr4_rx" } });
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
