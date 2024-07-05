#ifndef _GR4_PACKET_MODEM_PACKET_RECEIVER
#define _GR4_PACKET_MODEM_PACKET_RECEIVER

#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/binary_slicer.hpp>
#include <gnuradio-4.0/packet-modem/coarse_frequency_correction.hpp>
#include <gnuradio-4.0/packet-modem/constellation_llr_decoder.hpp>
#include <gnuradio-4.0/packet-modem/costas_loop.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_decoder.hpp>
#include <gnuradio-4.0/packet-modem/header_parser.hpp>
#include <gnuradio-4.0/packet-modem/header_payload_split.hpp>
#include <gnuradio-4.0/packet-modem/message_debug.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <gnuradio-4.0/packet-modem/payload_metadata_insert.hpp>
#include <gnuradio-4.0/packet-modem/symbol_filter.hpp>
#include <gnuradio-4.0/packet-modem/syncword_detection.hpp>
#include <gnuradio-4.0/packet-modem/syncword_remove.hpp>
#include <gnuradio-4.0/packet-modem/syncword_wipeoff.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/zmq_pdu_pub_sink.hpp>

namespace gr::packet_modem {

class PacketReceiver
{
public:
    gr::PortIn<std::complex<float>>* in;
    gr::PortOut<uint8_t>* out;

    PacketReceiver(gr::Graph& fg,
                   size_t samples_per_symbol = 4U,
                   const std::string& packet_len_tag_key = "packet_len",
                   bool header_debug = false,
                   bool zmq_output = false,
                   bool log = false)
    {
        using c64 = std::complex<float>;
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
        auto rrc_taps =
            firdes::root_raised_cosine(1.0,
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
            fg.emplaceBlock<SyncwordDetection>({ { "rrc_taps", rrc_taps },
                                                 { "syncword", syncword },
                                                 { "constellation", bpsk_constellation },
                                                 { "min_freq_bin", -4 },
                                                 { "max_freq_bin", 4 } });
        in = &syncword_detection.in;
        auto& freq_correction = fg.emplaceBlock<CoarseFrequencyCorrection<>>();
        const size_t symbol_filter_pfb_arms = 32UZ;
        // Build PFB RRC taps for symbol filter. The first arm of this PFB is
        // equal to rrc_taps. The gain of this filter is adjusted to achieve the
        // same amplitude as rrc_taps.
        auto rrc_taps_pfb = firdes::root_raised_cosine(
            static_cast<double>(symbol_filter_pfb_arms) /
                static_cast<double>(rrc_taps_norm),
            static_cast<double>(symbol_filter_pfb_arms * samples_per_symbol),
            1.0,
            0.35,
            symbol_filter_pfb_arms * samples_per_symbol * 11U);
        auto& symbol_filter = fg.emplaceBlock<SymbolFilter<c64, c64, float>>(
            { { "taps", rrc_taps_pfb },
              { "num_arms", symbol_filter_pfb_arms },
              { "samples_per_symbol", samples_per_symbol } });
        std::vector<float> syncword_bipolar;
        for (auto x : syncword) {
            syncword_bipolar.push_back(x ? -1.0f : 1.0f);
        }
        auto& syncword_wipeoff =
            fg.emplaceBlock<SyncwordWipeoff<>>({ { "syncword", syncword_bipolar } });
        auto& payload_metadata_insert =
            fg.emplaceBlock<PayloadMetadataInsert<>>({ { "log", log } });
        auto& costas_loop = fg.emplaceBlock<CostasLoop<>>();
        auto& syncword_remove = fg.emplaceBlock<SyncwordRemove<>>();
        // noise_sigma set for an Es/N0 of 0 dB, which is the worst design case
        // for header decoding
        auto& constellation_decoder = fg.emplaceBlock<ConstellationLLRDecoder<>>(
            { { "noise_sigma", 0.7f }, { "constellation", "QPSK" } });
        auto& descrambler = fg.emplaceBlock<AdditiveScrambler<float>>(
            { { "mask", uint64_t{ 0x4001U } },
              { "seed", uint64_t{ 0x18E38U } },
              { "length", uint64_t{ 16U } },
              { "reset_tag_key", "header_start" } });
        auto& header_payload_split = fg.emplaceBlock<HeaderPayloadSplit<>>(
            { { "packet_len_tag_key", packet_len_tag_key } });
        auto& header_fec_decoder = fg.emplaceBlock<HeaderFecDecoder>();
        auto& header_parser = fg.emplaceBlock<HeaderParser<>>();
        auto& payload_slicer = fg.emplaceBlock<BinarySlicer<true>>();
        auto& payload_pack =
            fg.emplaceBlock<PackBits<>>({ { "inputs_per_output", 8UZ },
                                          { "bits_per_input", uint8_t{ 1 } },
                                          { "packet_len_tag_key", packet_len_tag_key } });
        auto& payload_crc_check = fg.emplaceBlock<CrcCheck<>>(
            { { "discard_crc", true }, { "packet_len_tag_key", packet_len_tag_key } });
        out = &payload_crc_check.out;

        constexpr auto connection_error = "connection_error";

        if (header_debug) {
            auto& message_debug = fg.emplaceBlock<gr::packet_modem::MessageDebug>();
            if (header_parser.metadata.connect(message_debug.print) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
        }

        if (zmq_output) {
            auto& symbols_split = fg.emplaceBlock<HeaderPayloadSplit<c64>>(
                { { "header_size", 128UZ },
                  { "payload_length_key", "payload_symbols" } });
            auto& header_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<c64>>();
            auto& payload_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<c64>>();
            auto& zmq_header_sink =
                fg.emplaceBlock<ZmqPduPubSink<c64>>({ { "endpoint", "tcp://*:5000" } });
            auto& zmq_payload_sink =
                fg.emplaceBlock<ZmqPduPubSink<c64>>({ { "endpoint", "tcp://*:5001" } });
            if (fg.connect<"out">(syncword_remove).to<"in">(symbols_split) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"header">(symbols_split).to<"in">(header_to_pdu) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"payload">(symbols_split).to<"in">(payload_to_pdu) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(header_to_pdu).to<"in">(zmq_header_sink) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(payload_to_pdu).to<"in">(zmq_payload_sink) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
        }

        if (fg.connect<"out">(syncword_detection).to<"in">(freq_correction) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(freq_correction).to<"in">(symbol_filter) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(symbol_filter).to<"in">(syncword_wipeoff) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(syncword_wipeoff).to<"in">(payload_metadata_insert) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(payload_metadata_insert).to<"in">(costas_loop) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(costas_loop).to<"in">(syncword_remove) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(syncword_remove).to<"in">(constellation_decoder) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(constellation_decoder).to<"in">(descrambler) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(descrambler).to<"in">(header_payload_split) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"header">(header_payload_split).to<"in">(header_fec_decoder) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(header_fec_decoder).to<"in">(header_parser) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"metadata">(header_parser)
                .to<"parsed_header">(payload_metadata_insert) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"payload">(header_payload_split).to<"in">(payload_slicer) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(payload_slicer).to<"in">(payload_pack) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(payload_pack).to<"in">(payload_crc_check) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
    }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PACKET_RECEIVER
