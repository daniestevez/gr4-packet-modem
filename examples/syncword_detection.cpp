#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/binary_slicer.hpp>
#include <gnuradio-4.0/packet-modem/constellation_llr_decoder.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_decoder.hpp>
#include <gnuradio-4.0/packet-modem/header_parser.hpp>
#include <gnuradio-4.0/packet-modem/header_payload_split.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/packet_transmitter_pdu.hpp>
#include <gnuradio-4.0/packet-modem/payload_metadata_insert.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/probe_rate.hpp>
#include <gnuradio-4.0/packet-modem/rotator.hpp>
#include <gnuradio-4.0/packet-modem/symbol_filter.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
#include <gnuradio-4.0/packet-modem/syncword_remove.hpp>
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
    const std::vector<size_t> packet_lengths = { 10,  25,   100,  1500, 27,   38, 243,
                                                 514, 1500, 1500, 1024, 1024, 42, 34 };
    auto& vector_source =
        fg.emplaceBlock<gr::packet_modem::VectorSource<gr::packet_modem::Pdu<uint8_t>>>(
            { { "repeat", true } });
    for (auto len : packet_lengths) {
        std::vector<uint8_t> v(len);
        std::iota(v.begin(), v.end(), 0);
        vector_source.data.emplace_back(std::move(v));
    }
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
        fg.emplaceBlock<gr::packet_modem::Rotator<>>({ { "phase_incr", 0.0f } });
    auto& head =
        fg.emplaceBlock<gr::packet_modem::Head<c64>>({ { "num_items", 1000000UZ } });

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
    // normalize RRC taps to unity RMS norm
    float rrc_taps_norm = 0.0f;
    for (auto x : rrc_taps) {
        rrc_taps_norm += x * x;
    }
    rrc_taps_norm = std::sqrt(rrc_taps_norm);
    for (auto& x : rrc_taps) {
        x /= rrc_taps_norm;
    }
    const std::vector<c64> bpsk_constellation = { { 1.0f, 0.0f }, { -1.0f, 0.0f } };
    auto& syncword_detection =
        fg.emplaceBlock<gr::packet_modem::SyncwordDetection>({ { "rrc_taps", rrc_taps },
                                                               { "syncword", syncword },
                                                               { "min_freq_bin", -4 },
                                                               { "max_freq_bin", 4 } });
    syncword_detection.constellation = bpsk_constellation;
    auto& symbol_filter =
        fg.emplaceBlock<gr::packet_modem::SymbolFilter<c64, c64, float>>(
            { { "taps", rrc_taps }, { "samples_per_symbol", samples_per_symbol } });
    auto& payload_metadata_insert =
        fg.emplaceBlock<gr::packet_modem::PayloadMetadataInsert<>>();
    auto& syncword_remove = fg.emplaceBlock<gr::packet_modem::SyncwordRemove<>>();
    auto& constellation_decoder =
        fg.emplaceBlock<gr::packet_modem::ConstellationLLRDecoder<>>(
            { { "noise_sigma", 0.1f }, { "constellation", "QPSK" } });
    auto& descrambler = fg.emplaceBlock<gr::packet_modem::AdditiveScrambler<float>>(
        { { "mask", uint64_t{ 0x4001U } },
          { "seed", uint64_t{ 0x18E38U } },
          { "length", uint64_t{ 16U } },
          { "reset_tag_key", "header_start" } });
    auto& header_payload_split =
        fg.emplaceBlock<gr::packet_modem::HeaderPayloadSplit<>>();
    auto& header_fec_decoder = fg.emplaceBlock<gr::packet_modem::HeaderFecDecoder<>>();
    auto& header_parser = fg.emplaceBlock<gr::packet_modem::HeaderParser<>>();
    auto& payload_slicer = fg.emplaceBlock<gr::packet_modem::BinarySlicer<true>>();
    auto& payload_pack = fg.emplaceBlock<gr::packet_modem::PackBits<>>(
        { { "inputs_per_output", 8UZ },
          { "bits_per_input", uint8_t{ 1 } },
          { "packet_len_tag_key", "packet_len" } });
    auto& payload_crc_check =
        fg.emplaceBlock<gr::packet_modem::CrcCheck<>>({ { "discard_crc", true } });

    auto& file_sink = fg.emplaceBlock<gr::packet_modem::FileSink<uint8_t>>(
        { { "filename", "syncword_detection.u8" } });
    auto& header_file_sink = fg.emplaceBlock<gr::packet_modem::FileSink<uint8_t>>(
        { { "filename", "header_syncword_detection.u8" } });
    auto& vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();
    auto& header_vector_sink = fg.emplaceBlock<gr::packet_modem::VectorSink<uint8_t>>();

    expect(eq(gr::ConnectionResult::SUCCESS,
              vector_source.out.connect(*packet_transmitter_pdu.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              packet_transmitter_pdu.out_packet->connect(pdu_to_stream.in)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(pdu_to_stream).to<"in">(rotator)));
    expect(eq(gr::ConnectionResult::SUCCESS, fg.connect<"out">(rotator).to<"in">(head)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(head).to<"in">(syncword_detection)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(syncword_detection).to<"in">(symbol_filter)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(symbol_filter).to<"in">(payload_metadata_insert)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(payload_metadata_insert).to<"in">(syncword_remove)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(syncword_remove).to<"in">(constellation_decoder)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(constellation_decoder).to<"in">(descrambler)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(descrambler).to<"in">(header_payload_split)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"header">(header_payload_split).to<"in">(header_fec_decoder)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(header_fec_decoder).to<"in">(header_file_sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(header_fec_decoder).to<"in">(header_vector_sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(header_fec_decoder).to<"in">(header_parser)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"metadata">(header_parser)
                  .to<"parsed_header">(payload_metadata_insert)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"payload">(header_payload_split).to<"in">(payload_slicer)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(payload_slicer).to<"in">(payload_pack)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(payload_pack).to<"in">(payload_crc_check)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(payload_crc_check).to<"in">(file_sink)));
    expect(eq(gr::ConnectionResult::SUCCESS,
              fg.connect<"out">(payload_crc_check).to<"in">(vector_sink)));

    gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::singleThreaded> sched{
        std::move(fg)
    };
    gr::MsgPortOut toScheduler;
    expect(eq(gr::ConnectionResult::SUCCESS, toScheduler.connect(sched.msgIn)));
    std::thread stopper([&toScheduler]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        gr::sendMessage<gr::message::Command::Set>(toScheduler,
                                                   "",
                                                   gr::block::property::kLifeCycleState,
                                                   { { "state", "REQUESTED_STOP" } });
    });

    const auto ret = sched.runAndWait();
    stopper.join();
    expect(ret.has_value());
    if (!ret.has_value()) {
        fmt::println("scheduler error: {}", ret.error());
    }

    fmt::println("HEADER TAGS");
    for (const auto& _tag : header_vector_sink.tags()) {
        fmt::println("offset = {}, map = {}", _tag.index, _tag.map);
    }
    fmt::println("PAYLOAD TAGS");
    for (const auto& _tag : vector_sink.tags()) {
        fmt::println("offset = {}, map = {}", _tag.index, _tag.map);
    }

    return 0;
}
