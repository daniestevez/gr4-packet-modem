#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/random_source.hpp>
#include <gnuradio-4.0/packet-modem/unpack_bits.hpp>
#include <boost/ut.hpp>
#include <cmath>
#include <complex>

int main(int argc, char* argv[])
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    // The first command line argument of this example indicates the file to
    // write the output to.
    expect(fatal(argc == 2));

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::RandomSource<uint8_t>>(
        { { "minimum", uint8_t{ 0 } },
          { "maximum", uint8_t{ 255 } },
          { "num_items", 1000000UZ },
          { "repeat", true } });
    auto& unpack = fg.emplaceBlock<gr::packet_modem::UnpackBits<>>(
        { { "outputs_per_input", 4UZ }, { "bits_per_output", uint8_t{ 2U } } });
    const float a = std::sqrt(2.0f) / 2.0f;
    const std::vector<c64> qpsk_constellation = {
        { a, a }, { a, -a }, { -a, a }, { -a, -a }
    };
    auto& constellation_modulator =
        fg.emplaceBlock<gr::packet_modem::Mapper<uint8_t, c64>>(
            { { "map", qpsk_constellation } });
    const size_t sps = 4;
    const size_t ntaps = sps * 11;
    const auto rrc_taps = gr::packet_modem::firdes::root_raised_cosine(
        1.0, static_cast<double>(sps), 1.0, 0.35, ntaps);
    auto& rrc_interp =
        fg.emplaceBlock<gr::packet_modem::InterpolatingFirFilter<c64, c64, float>>(
            { { "interpolation", sps }, { "taps", rrc_taps } });
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(unpack)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(unpack).to<"in">(constellation_modulator)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(constellation_modulator).to<"in">(rrc_interp)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(rrc_interp).to<"in">(sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(rrc_interp).to<"in">(probe_rate)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, probe_rate.rate.connect(message_debug.print)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded> sched{ std::move(
        fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
