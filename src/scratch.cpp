#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <pmtv/pmt.hpp>
#include <boost/ut.hpp>
#include <chrono>
#include <numeric>
#include <thread>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;

    const std::vector<uint8_t> v = {0, 1, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1};
    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<uint8_t>>(v);
    auto& pack = fg.emplaceBlock<gr::packet_modem::PackBits<>>(2U);
    auto& sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(pack)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(pack).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));

    expect(sched.runAndWait().has_value());

    const auto data = sink.data();
    std::print("vector sink contains {} items\n", data.size());
    std::print("vector sink items:\n");
    for (const auto n : data) {
        std::print("{} ", n);
    }
    
    return 0;
}
