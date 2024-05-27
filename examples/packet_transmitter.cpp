#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_strobe.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char* argv[])
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    // The first command line argument of this example indicates the file to
    // write the output to.
    expect(fatal(argc == 2));

    gr::Graph fg;
    // auto& random_source = fg.emplaceBlock<gr::packet_modem::RandomSource<uint8_t>>(
    //     uint8_t{ 0 }, uint8_t{ 255 }, 1000000UZ);
    // random_source.name = "RandomSource";
    const size_t packet_length = 1500;
    // auto& stream_to_tagged =
    //     fg.emplaceBlock<gr::packet_modem::StreamToTaggedStream<uint8_t>>(packet_length);
    auto& source = fg.emplaceBlock<gr::packet_modem::PacketStrobe<uint8_t>>(
        packet_length, std::chrono::milliseconds(100), "packet_len", false);
    auto packet_transmitter = gr::packet_modem::PacketTransmitter(fg);
    auto& packet_to_stream = fg.emplaceBlock<gr::packet_modem::PacketToStream<c64>>();
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(100e3, 1000U);
    auto& sink = fg.emplaceBlock<gr::packet_modem::FileSink<c64>>(argv[1]);
    auto& probe_rate =
        fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>(std::chrono::seconds(1));
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    // expect(eq(gr::ConnectionResult::SUCCESS,
    //           fg.connect<"out">(random_source).to<"in">(stream_to_tagged)));
    // expect(eq(gr::ConnectionResult::SUCCESS,
    //           stream_to_tagged.out.connect(*packet_transmitter.in)));
    expect(eq(gr::ConnectionResult::SUCCESS, source.out.connect(*packet_transmitter.in)));
    // expect(eq(gr::ConnectionResult::SUCCESS,
    // packet_transmitter.out->connect(sink.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter.out->connect(packet_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(sink)));
    // expect(eq(gr::ConnectionResult::SUCCESS,
    //           packet_transmitter.out->connect(probe_rate.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(throttle).to<"in">(probe_rate)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, probe_rate.rate.connect(message_debug.print)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
