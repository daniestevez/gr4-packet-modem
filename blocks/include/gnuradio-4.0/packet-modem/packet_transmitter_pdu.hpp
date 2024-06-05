#ifndef _GR4_PACKET_MODEM_PACKET_TRANSMITTER_PDU
#define _GR4_PACKET_MODEM_PACKET_TRANSMITTER_PDU

#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/burst_shaper.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/glfsr_source.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_encoder.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/multiply_packet_len_tag.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <gnuradio-4.0/packet-modem/packet_ingress.hpp>
#include <gnuradio-4.0/packet-modem/packet_mux.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/stream_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/unpack_bits.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <cstdint>
#include <numbers>

namespace gr::packet_modem {

class PacketTransmitterPdu
{
public:
    gr::PortIn<Pdu<uint8_t>>* in;
    gr::PortOut<std::complex<float>>* out;

    PacketTransmitterPdu(gr::Graph& fg,
                         bool stream_mode = false,
                         size_t samples_per_symbol = 4U)
    {
        using c64 = std::complex<float>;

        auto& ingress = fg.emplaceBlock<PacketIngress<Pdu<uint8_t>>>();
        in = &ingress.in;

        // header
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<Pdu<uint8_t>>>();
        auto& header_fec = fg.emplaceBlock<HeaderFecEncoder<Pdu<uint8_t>>>();

        // payload
        auto& crc_append = fg.emplaceBlock<CrcAppend<Pdu<uint8_t>>>();
        // a payload FEC encoder block would be instantiated here

        auto& header_payload_mux =
            fg.emplaceBlock<PacketMux<Pdu<uint8_t>>>({ { "num_inputs", 2UZ } });
        header_payload_mux.name = "PacketTransmitter(header_payload_mux)";

        auto& scrambler_unpack =
            fg.emplaceBlock<UnpackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "outputs_per_input", 8UZ } });
        // 17-bit CCSDS scrambler defined in CCSDS 131.0-B-5 (September 2023)
        auto& scrambler = fg.emplaceBlock<AdditiveScrambler<Pdu<uint8_t>>>(
            { { "mask", uint64_t{ 0x4001U } },
              { "seed", uint64_t{ 0x18E38U } },
              { "length", uint64_t{ 16U } } });
        const float a = std::sqrt(2.0f) / 2.0f;
        const std::vector<c64> qpsk_constellation = {
            { a, a }, { a, -a }, { -a, a }, { -a, -a }
        };
        auto& qpsk_pack =
            fg.emplaceBlock<PackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "inputs_per_output", 2UZ }, { "bits_per_input", uint8_t{ 1 } } });
        auto& qpsk_modulator = fg.emplaceBlock<Mapper<Pdu<uint8_t>, Pdu<c64>>>();
        qpsk_modulator.map = qpsk_constellation;

        // syncword (64-bit CCSDS syncword)
        const Pdu<uint8_t> syncword_pdu = {
            { uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 },
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
              uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 }, uint8_t{ 0 } },
            {}
        };
        auto& syncword_source =
            fg.emplaceBlock<VectorSource<Pdu<uint8_t>>>({ { "repeat", true } });
        syncword_source.data = std::vector<Pdu<uint8_t>>{ syncword_pdu };
        syncword_source.name = "PacketTransmitter(syncword_source)";
        const std::vector<c64> bpsk_constellation = { { 1.0f, 0.0f }, { -1.0f, 0.0f } };
        auto& syncword_bpsk_modulator = fg.emplaceBlock<Mapper<Pdu<uint8_t>, Pdu<c64>>>();
        syncword_bpsk_modulator.map = bpsk_constellation;

        auto& symbols_mux = fg.emplaceBlock<PacketMux<Pdu<c64>>>(
            { { "num_inputs", stream_mode ? 2UZ : 4UZ } });
        symbols_mux.name = "PacketTransmitter(symbols_mux)";

        constexpr auto connection_error = "connection_error";

        const size_t rrc_flush_nsymbols = 11;
        if (!stream_mode) {
            throw gr::exception("packet mode not supported yet");
            // // ramp-down sequence
            // auto& ramp_down_source = fg.emplaceBlock<GlfsrSource<>>({ { "degree", 32 }
            // });
            // // 9 symbols used for ramp down. 5 to clear the RRC filter and 4 to
            // // actually perform the amplitude ramp-down
            // const size_t ramp_down_nsymbols = 9;
            // const size_t ramp_down_nbits = 2U * ramp_down_nsymbols;
            // auto& ramp_down_tags = fg.emplaceBlock<StreamToTaggedStream<uint8_t>>(
            //     { { "packet_length", static_cast<uint64_t>(ramp_down_nbits) } });
            // auto& ramp_down_pack = fg.emplaceBlock<PackBits<>>(
            //     { { "inputs_per_output", 2UZ },
            //       { "bits_per_input", uint8_t{ 1 } },
            //       { "packet_len_tag_key", packet_len_tag_key } });
            // auto& ramp_down_modulator = fg.emplaceBlock<Mapper<uint8_t, c64>>();
            // ramp_down_modulator.map = qpsk_constellation;
            // auto& ramp_symbols_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<c64>>(
            //     { { "packet_len_tag_key", packet_len_tag_key } });
            // ramp_symbols_to_pdu.name = "PacketTransmitter(ramp_symbols_to_pdu)";

            // auto& rrc_flush_source = fg.emplaceBlock<NullSource<c64>>();
            // auto& rrc_flush_tags = fg.emplaceBlock<StreamToTaggedStream<c64>>(
            //     { { "packet_length", static_cast<uint64_t>(rrc_flush_nsymbols) } });
            // auto& flush_symbols_to_pdu = fg.emplaceBlock<TaggedStreamToPdu<c64>>(
            //     { { "packet_len_tag_key", packet_len_tag_key } });
            // flush_symbols_to_pdu.name = "PacketTransmitter(flush_symbols_to_pdu)";

            // if (fg.connect<"out">(ramp_down_source).to<"in">(ramp_down_tags) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect<"out">(ramp_down_tags).to<"in">(ramp_down_pack) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect<"out">(ramp_down_pack).to<"in">(ramp_down_modulator) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect<"out">(ramp_down_modulator).to<"in">(ramp_symbols_to_pdu) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect(ramp_symbols_to_pdu, { "out" }, symbols_mux, { "in", 2 }) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }

            // if (fg.connect<"out">(rrc_flush_source).to<"in">(rrc_flush_tags) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect<"out">(rrc_flush_tags).to<"in">(flush_symbols_to_pdu) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // if (fg.connect(flush_symbols_to_pdu, { "out" }, symbols_mux, { "in", 3 })
            // !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
        }

        const size_t ntaps = samples_per_symbol * 11U;
        auto rrc_taps = firdes::root_raised_cosine(
            1.0, static_cast<double>(samples_per_symbol), 1.0, 0.35, ntaps);
        // scale rrc_taps for maximum power using [-1, 1] DAC range
        std::vector<float> rrc_taps_sum_abs(samples_per_symbol);
        for (size_t j = 0; j < samples_per_symbol; ++j) {
            for (size_t k = j; k < rrc_taps.size(); k += samples_per_symbol) {
                rrc_taps_sum_abs[j] += std::abs(rrc_taps[k]);
            }
        }
        const float rrc_taps_sum_abs_max =
            *std::max_element(rrc_taps_sum_abs.cbegin(), rrc_taps_sum_abs.cend());
        // used to avoid reaching DAC full scale
        const float scale = 0.9f;
        for (auto& x : rrc_taps) {
            x *= scale / rrc_taps_sum_abs_max;
        }
        // do not produce tags in PduToTaggedStream
        auto& pdu_to_stream =
            fg.emplaceBlock<PduToTaggedStream<c64>>({ { "packet_len_tag_key", "" } });
        auto& rrc_interp = fg.emplaceBlock<InterpolatingFirFilter<c64, c64, float>>(
            { { "interpolation", samples_per_symbol }, { "taps", rrc_taps } });

        if (!stream_mode) {
            // // burst shaper
            // const size_t ramp_symbols = 4U;
            // const size_t ramp_samples = ramp_symbols * samples_per_symbol;
            // // offset to compensate group delay of RRC filter
            // const size_t offset = 4U * samples_per_symbol;
            // std::vector<float> leading_ramp(offset + ramp_samples);
            // for (size_t j = 0; j < ramp_samples; ++j) {
            //     leading_ramp[offset + j] = static_cast<float>(
            //         std::sin(static_cast<double>(j + 1) /
            //                  static_cast<double>(ramp_samples) * 0.5 *
            //                  std::numbers::pi));
            // }
            // std::vector<float> trailing_ramp(rrc_flush_nsymbols *
            // samples_per_symbol -
            //                                  offset + ramp_samples);
            // for (size_t j = 0; j < ramp_samples; ++j) {
            //     trailing_ramp[j] = leading_ramp[leading_ramp.size() - 1 - j];
            // }
            // auto& burst_shaper = fg.emplaceBlock<BurstShaper<c64, c64, float>>(
            //     { { "leading_shape", leading_ramp },
            //       { "trailing_shape", trailing_ramp },
            //       { "packet_len_tag_key", packet_len_tag_key } });
            // if (fg.connect<"out">(rrc_interp_mult_tag).to<"in">(burst_shaper) !=
            //     ConnectionResult::SUCCESS) {
            //     throw std::runtime_error(connection_error);
            // }
            // out = &burst_shaper.out;
        } else {
            out = &rrc_interp.out;
        }

        if (fg.connect<"out">(ingress).to<"in">(crc_append) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (ingress.metadata.connect(header_formatter.metadata) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(header_formatter).to<"in">(header_fec) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        // payload FEC connection would go here
        if (fg.connect(header_fec, { "out" }, header_payload_mux, { "in", 0 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(crc_append, { "out" }, header_payload_mux, { "in", 1 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(header_payload_mux).to<"in">(scrambler_unpack) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(scrambler_unpack).to<"in">(scrambler) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(scrambler).to<"in">(qpsk_pack) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(qpsk_pack).to<"in">(qpsk_modulator) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(syncword_source).to<"in">(syncword_bpsk_modulator) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(syncword_bpsk_modulator, { "out" }, symbols_mux, { "in", 0 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(qpsk_modulator, { "out" }, symbols_mux, { "in", 1 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(symbols_mux).to<"in">(pdu_to_stream) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(pdu_to_stream).to<"in">(rrc_interp) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
    }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PACKET_TRANSMITTER_PDU
