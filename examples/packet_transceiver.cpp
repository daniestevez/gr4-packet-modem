#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/noise_source.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_to_stream.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_resampler.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_taps.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/throttle.hpp>
#include <gnuradio-4.0/packet-modem/tun_sink.hpp>
#include <gnuradio-4.0/packet-modem/tun_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char** argv)
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    expect(fatal(eq(argc, 4)));
    const double esn0_db = std::stod(argv[1]);
    const float freq_error = std::stof(argv[2]);
    const float sfo_ppm = std::stof(argv[3]);

    const double tx_power = 0.32; // measured from packet_transmitter_pdu output
    const size_t samples_per_symbol = 4U;
    const double n0 = tx_power * static_cast<double>(samples_per_symbol) *
                      std::pow(10.0, -0.1 * esn0_db);
    const float noise_amplitude = static_cast<float>(std::sqrt(n0));

    const double samp_rate = 1e6;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::TunSource>(
        { { "tun_name", "gr4_tun_tx" }, { "netns_name", "gr4_tx" } });
    const bool stream_mode = false;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& packet_to_stream =
        fg.emplaceBlock<gr::packet_modem::PacketToStream<gr::packet_modem::Pdu<c64>>>();
    // Limit the number of samples that packet_to_stream can produce in one
    // call. Otherwise it produces 65536 items on the first call, and then "it
    // gets behind the Throttle block" by these many samples.
    packet_to_stream.out.max_samples = 1000U;
    auto& throttle = fg.emplaceBlock<gr::packet_modem::Throttle<c64>>(
        { { "sample_rate", samp_rate }, { "maximum_items_per_chunk", 1000UZ } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    auto& resampler = fg.emplaceBlock<gr::packet_modem::PfbArbResampler<c64, c64, float>>(
        { { "taps", gr::packet_modem::pfb_arb_taps },
          { "rate", 1.0f + 1e-6f * sfo_ppm } });
    auto& rotator =
        fg.emplaceBlock<gr::packet_modem::Rotator<>>({ { "phase_incr", freq_error } });
    auto& noise_source = fg.emplaceBlock<gr::packet_modem::NoiseSource<c64>>(
        { { "noise_type", "gaussian" }, { "amplitude", noise_amplitude } });
    auto& add_noise = fg.emplaceBlock<gr::packet_modem::Add<c64>>();
    const bool header_debug = false;
    const bool zmq_output = true;
    const bool log = true;
    auto packet_receiver = gr::packet_modem::PacketReceiver(
        fg, samples_per_symbol, "packet_len", header_debug, zmq_output, log);
    auto& tag_to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<uint8_t>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::TunSink>(
        { { "tun_name", "gr4_tun_rx" }, { "netns_name", "gr4_rx" } });

    expect(eq(gr::ConnectionResult::SUCCESS,
              source.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(packet_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_to_stream).to<"in">(resampler)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(resampler).to<"in">(throttle)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(throttle).to<"in">(probe_rate)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, probe_rate.rate.connect(message_debug.print)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(throttle).to<"in">(rotator)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(rotator).to<"in0">(add_noise)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(noise_source).to<"in1">(add_noise)));
    expect(eq(gr::ConnectionResult::SUCCESS, add_noise.out.connect(*packet_receiver.in)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, packet_receiver.out->connect(tag_to_pdu.in)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(tag_to_pdu).to<"in">(sink)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    expect(ret.has_value());
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    return 0;
}
