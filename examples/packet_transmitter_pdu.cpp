#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
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
    const uint64_t packet_length = 1500;
    const std::vector<uint8_t> v(packet_length);
    const gr::packet_modem::Pdu<uint8_t> pdu = { v, {} };
    auto& vector_source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<uint8_t>>>(
            { { "repeat", true } });
    vector_source.data = std::vector<gr::packet_modem::Pdu<uint8_t>>{ pdu };
    const bool stream_mode = false;
    const size_t samples_per_symbol = 4U;
    const size_t max_in_samples = 1U;
    // note that buffer size is rounded up to a multiple of
    // lcm(sizeof(Pdu<T>), getpagesize()), but different values that round up to
    // the same number give slightly different performance
    const size_t out_buff_size = 1U;
    auto packet_transmitter_pdu = gr::packet_modem::PacketTransmitterPdu(
        fg, stream_mode, samples_per_symbol, max_in_samples, out_buff_size);
    auto& sink =
        fg.emplaceBlock<gr::packet_modem::FileSink<c64>>({ { "filename", argv[1] } });
    auto& probe_rate = fg.emplaceBlock<gr::packet_modem::ProbeRate<c64>>();

    expect(eq(gr::ConnectionResult::SUCCESS,
              vector_source.out.connect(*packet_transmitter_pdu.in)));
    if (!stream_mode) {
        // do not produce tags in PduToTaggedStream
        auto& pdu_to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<c64>>(
            { { "packet_len_tag_key", "" } });
        if (max_in_samples) {
            pdu_to_stream.in.max_samples = max_in_samples;
        }
        expect(eq(gr::ConnectionResult::SUCCESS,
                  packet_transmitter_pdu.out_packet->connect(pdu_to_stream.in)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_stream).to<"in">(sink)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  fg.connect<"out">(pdu_to_stream).to<"in">(probe_rate)));
    } else {
        expect(eq(gr::ConnectionResult::SUCCESS,
                  packet_transmitter_pdu.out_stream->connect(sink.in)));
        expect(eq(gr::ConnectionResult::SUCCESS,
                  packet_transmitter_pdu.out_stream->connect(probe_rate.in)));
    }
    auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
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
