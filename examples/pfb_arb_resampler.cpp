#include <fmt/core.h>
#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_resampler.hpp>
#include <gnuradio-4.0/packet-modem/pfb_arb_taps.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <numbers>

int main()
{
    using namespace boost::ut;

    gr::Graph fg;
    auto& source = fg.emplaceBlock<gr::packet_modem::VectorSource<float>>();
    const size_t num_input_items = 16384;
    const float freq = 0.01f;
    float phase = 0.0f;
    source.data.clear();
    for (size_t j = 0; j < num_input_items; ++j) {
        source.data.push_back(std::cos(phase));
        phase += freq;
        if (phase > std::numbers::pi_v<float>) {
            phase -= 2.0f * std::numbers::pi_v<float>;
        }
    }
    auto& resampler = fg.emplaceBlock<gr::packet_modem::PfbArbResampler<float>>(
        { { "taps", gr::packet_modem::pfb_arb_taps }, { "rate", 1.123456789f } });
    auto& sink = fg.emplaceBlock<gr::packet_modem::FileSink<float>>(
        { { "filename", "pfb_arb_resampler_out.f32" } });
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(source).to<"in">(resampler)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(resampler).to<"in">(sink)));

    gr::scheduler::Simple sched{ std::move(fg) };
    expect(sched.runAndWait().has_value());

    return 0;
}
