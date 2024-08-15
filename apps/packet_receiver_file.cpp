#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_source.hpp>
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

    expect(fatal(eq(argc, 2)));

    gr::Graph fg;
    auto& file_source =
        fg.emplaceBlock<gr::packet_modem::FileSource<c64>>({ { "filename", argv[1] } });
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
           fg.connect(file_source, "out"s, *packet_receiver.syncword_detection, "in"s)));
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
