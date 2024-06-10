#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
#include <gnuradio-4.0/packet-modem/vector_sink.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <boost/ut.hpp>
#include <complex>
#include <cstdint>

int main()
{
    using namespace boost::ut;
    using c64 = std::complex<float>;

    gr::Graph fg;
    const size_t packet_length = 1500;
    const gr::packet_modem::Pdu<uint8_t> pdu = { std::vector<uint8_t>(packet_length),
                                                 {} };
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
    auto& pdu_to_stream = fg.emplaceBlock<gr::packet_modem::PduToTaggedStream<c64>>(
        { { "packet_len_tag_key", "" } });
    if (max_in_samples) {
        pdu_to_stream.in.max_samples = max_in_samples;
    }
    auto& rotator =
        fg.emplaceBlock<gr::packet_modem::Rotator<>>({ { "phase_incr", 0.025f } });

    const std::vector<uint8_t> syncword = {
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
        uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 0 },
        uint8_t{ 1 }, uint8_t{ 1 }, uint8_t{ 0 }, uint8_t{ 1 }, uint8_t{ 1 },
        uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }
    };
    auto rrc_taps = gr::packet_modem::firdes::root_raised_cosine(
        1.0,
        static_cast<double>(samples_per_symbol),
        1.0,
        0.35,
        samples_per_symbol * 11U);
    const std::vector<c64> bpsk_constellation = { { 1.0f, 0.0f }, { -1.0f, 0.0f } };
    auto& syncword_detection =
        fg.emplaceBlock<gr::packet_modem::SyncwordDetection>({ { "rrc_taps", rrc_taps },
                                                               { "syncword", syncword },
                                                               { "min_freq_bin", -4 },
                                                               { "max_freq_bin", 4 } });
    syncword_detection.constellation = bpsk_constellation;

    auto& head =
        fg.emplaceBlock<gr::packet_modem::Head<c64>>({ { "num_items", 1000000UZ } });
    auto& file_sink = fg.emplaceBlock<gr::packet_modem::FileSink<c64>>(
        { { "filename", "syncword_detection.c64" } });
    auto& vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<c64>>();

    expect(eq(gr::ConnectionResult::SUCCESS,
              vector_source.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(pdu_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(pdu_to_stream).to<"in">(rotator)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(rotator).to<"in">(syncword_detection)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(syncword_detection).to<"in">(head)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(file_sink)));
    expect(
        eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(head).to<"in">(vector_sink)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    const auto ret = sched.runAndWait();
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    fmt::println("TAGS");
    for (const auto& _tag : vector_sink.tags()) {
        fmt::println("offset = {}, map = {}", _tag.index, _tag.map);
    }

    return 0;
}
