#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char* argv[])
{
    using namespace boost::ut;
    using c64 = std::complex<float>;
    using namespace std::string_literals;

    // The first command line argument of this example indicates the file to
    // write the output to.
    expect(fatal(argc == 2));

    gr::Graph fg;
    auto& random_source = fg.emplaceBlock<gr::packet_modem::RandomSource<uint8_t>>(
        { { "minimum", uint8_t{ 0 } },
          { "maximum", uint8_t{ 255 } },
          { "num_items", 1000000UZ },
          { "repeat", true } });
    random_source.name = "RandomSource";
    const uint64_t packet_length = 1500;
    auto& stream_to_tagged =
        fg.emplaceBlock<gr::packet_modem::StreamToTaggedStream<uint8_t>>(
            { { "packet_length", packet_length } });
    auto packet_transmitter = gr::packet_modem::PacketTransmitter(fg);
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(random_source).to<"in">(stream_to_tagged)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(stream_to_tagged, "out"s, *packet_transmitter.ingress, "in"s)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(*packet_transmitter.burst_shaper, "out"s, sink, "in"s)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(*packet_transmitter.burst_shaper, "out"s, probe_rate, "in"s)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect(probe_rate, "rate"s, message_debug, "print"s)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
