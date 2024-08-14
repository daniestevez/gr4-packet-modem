#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/packet_receiver.hpp>
#include <gnuradio-4.0/packet-modem/packet_type_filter.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/tun_sink.hpp>
#include <gnuradio-4.0/soapy/Soapy.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main(int argc, char** argv)
{
    using namespace boost::ut;
    using c64 = std::complex<float>;
    using namespace std::string_literals;

    expect(fatal(eq(argc, 2) or eq(argc, 3)));
    const double rf_freq = std::stod(argv[1]);
    const float samp_rate = argc == 2 ? 3.2e6 : std::stof(argv[2]);

    gr::Graph fg;
    auto& soapy_source = fg.emplaceBlock<gr::blocks::soapy::SoapyBlock<c64, 1UZ>>(
        { { "device", "rtlsdr" },
          { "sample_rate", samp_rate },
          { "rx_center_frequency", std::vector<double>{ rf_freq } },
          { "rx_gains", std::vector<double>{ 30.0 } } });
    const size_t samples_per_symbol = 4;
    const bool header_debug = false;
    const bool zmq_output = true;
    const bool log = true;
    auto packet_receiver = gr::packet_modem::PacketReceiver(
        fg, samples_per_symbol, "packet_len", header_debug, zmq_output, log);
    auto& packet_type_filter = fg.emplaceBlock<gr::packet_modem::PacketTypeFilter<>>(
        { { "packet_type", "user_data" } });
    auto& tag_to_pdu = fg.emplaceBlock<gr::packet_modem::TaggedStreamToPdu<uint8_t>>();
    auto& sink = fg.emplaceBlock<gr::packet_modem::TunSink>(
        { { "tun_name", "gr4_tun_rx" }, { "netns_name", "gr4_rx" } });

    expect(
        eq(gr::ConnectionResult::SUCCESS,
           fg.connect(soapy_source, "out"s, *packet_receiver.syncword_detection, "in"s)));
    expect(
        eq(gr::ConnectionResult::SUCCESS,
           fg.connect(
               *packet_receiver.payload_crc_check, "out"s, packet_type_filter, "in"s)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(packet_type_filter).to<"in">(tag_to_pdu)));
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
