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
#include <gnuradio-4.0/packet-modem/stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/unpack_bits.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <cstdint>
#include <numbers>

namespace gr::packet_modem {

class PacketTransmitterPdu
{
public:
    using c64 = std::complex<float>;
    gr::PortIn<Pdu<uint8_t>>* in;
    // only used when stream_mode = true
    gr::PortOut<c64>* out_stream;
    gr::PortOut<gr::Message, gr::Async, gr::Optional>* out_count;
    // only used when stream_mode = false
    gr::PortOut<Pdu<c64>>* out_packet;

    PacketTransmitterPdu(gr::Graph& fg,
                         bool stream_mode = false,
                         size_t samples_per_symbol = 4U,
                         size_t max_in_samples = 0U,
                         size_t out_buff_size = 0U)
    {
        auto& ingress = fg.emplaceBlock<PacketIngress<Pdu<uint8_t>>>();
        if (max_in_samples) {
            ingress.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (ingress.out.resizeBuffer(out_buff_size) != ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        in = &ingress.in;

        // header
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter<Pdu<uint8_t>>>();
        if (max_in_samples) {
            header_formatter.metadata.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (header_formatter.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        auto& header_fec = fg.emplaceBlock<HeaderFecEncoder<Pdu<uint8_t>>>();
        if (max_in_samples) {
            header_fec.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (header_fec.out.resizeBuffer(out_buff_size) != ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }

        // payload
        auto& crc_append = fg.emplaceBlock<CrcAppend<Pdu<uint8_t>>>();
        if (max_in_samples) {
            crc_append.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (crc_append.out.resizeBuffer(out_buff_size) != ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        // a payload FEC encoder block would be instantiated here

        auto& header_payload_mux =
            fg.emplaceBlock<PacketMux<Pdu<uint8_t>>>({ { "num_inputs", 2UZ } });
        header_payload_mux.name = "PacketTransmitter(header_payload_mux)";
        if (max_in_samples) {
            header_payload_mux.in.at(0).max_samples = max_in_samples;
            header_payload_mux.in.at(1).max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (header_payload_mux.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }

        auto& scrambler_unpack =
            fg.emplaceBlock<UnpackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "outputs_per_input", 8UZ } });
        if (max_in_samples) {
            scrambler_unpack.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (scrambler_unpack.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        // 17-bit CCSDS scrambler defined in CCSDS 131.0-B-5 (September 2023)
        auto& scrambler = fg.emplaceBlock<AdditiveScrambler<Pdu<uint8_t>>>(
            { { "mask", uint64_t{ 0x4001U } },
              { "seed", uint64_t{ 0x18E38U } },
              { "length", uint64_t{ 16U } } });
        if (max_in_samples) {
            scrambler.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (scrambler.out.resizeBuffer(out_buff_size) != ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        const float a = std::sqrt(2.0f) / 2.0f;
        const std::vector<c64> qpsk_constellation = {
            { a, a }, { a, -a }, { -a, a }, { -a, -a }
        };
        auto& qpsk_pack =
            fg.emplaceBlock<PackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                { { "inputs_per_output", 2UZ }, { "bits_per_input", uint8_t{ 1 } } });
        if (max_in_samples) {
            qpsk_pack.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (qpsk_pack.out.resizeBuffer(out_buff_size) != ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }
        auto& qpsk_modulator = fg.emplaceBlock<Mapper<Pdu<uint8_t>, Pdu<c64>>>(
            { { "map", qpsk_constellation } });
        if (max_in_samples) {
            qpsk_modulator.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (qpsk_modulator.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }

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
        auto& syncword_bpsk_modulator = fg.emplaceBlock<Mapper<Pdu<uint8_t>, Pdu<c64>>>(
            { { "map", bpsk_constellation } });
        if (max_in_samples) {
            syncword_bpsk_modulator.in.max_samples = max_in_samples;
        }
        if (out_buff_size) {
            if (syncword_bpsk_modulator.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }

        auto& symbols_mux = fg.emplaceBlock<PacketMux<Pdu<c64>>>(
            { { "num_inputs", stream_mode ? 2UZ : 4UZ } });
        symbols_mux.name = "PacketTransmitter(symbols_mux)";
        if (max_in_samples) {
            for (auto& in_port : symbols_mux.in) {
                in_port.max_samples = max_in_samples;
            }
        }
        if (out_buff_size) {
            if (symbols_mux.out.resizeBuffer(out_buff_size) !=
                ConnectionResult::SUCCESS) {
                throw gr::exception("resizeBuffer() failed");
            }
        }

        constexpr auto connection_error = "connection_error";

        const size_t rrc_flush_nsymbols = 11;
        if (!stream_mode) {
            // ramp-down sequence
            auto& ramp_down_source = fg.emplaceBlock<GlfsrSource<>>({ { "degree", 32 } });
            // 9 symbols used for ramp down. 5 to clear the RRC filter and 4 to
            // actually perform the amplitude ramp-down
            const size_t ramp_down_nsymbols = 9;
            const size_t ramp_down_nbits = 2U * ramp_down_nsymbols;
            auto& ramp_down_to_pdu = fg.emplaceBlock<StreamToPdu<uint8_t>>(
                { { "packet_length", ramp_down_nbits } });
            if (out_buff_size) {
                if (ramp_down_to_pdu.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }
            auto& ramp_down_pack =
                fg.emplaceBlock<PackBits<Endianness::MSB, Pdu<uint8_t>, Pdu<uint8_t>>>(
                    { { "inputs_per_output", 2UZ }, { "bits_per_input", uint8_t{ 1 } } });
            if (max_in_samples) {
                ramp_down_pack.in.max_samples = max_in_samples;
            }
            if (out_buff_size) {
                if (ramp_down_pack.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }
            auto& ramp_down_modulator = fg.emplaceBlock<Mapper<Pdu<uint8_t>, Pdu<c64>>>(
                { { "map", qpsk_constellation } });
            if (max_in_samples) {
                ramp_down_modulator.in.max_samples = max_in_samples;
            }
            if (out_buff_size) {
                if (ramp_down_modulator.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }

            const std::vector<c64> flush_vector(rrc_flush_nsymbols);
            const Pdu<c64> flush_pdu = { flush_vector, {} };
            auto& rrc_flush_source =
                fg.emplaceBlock<VectorSource<Pdu<c64>>>({ { "repeat", true } });
            if (out_buff_size) {
                if (rrc_flush_source.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }
            rrc_flush_source.data = std::vector<Pdu<c64>>{ flush_pdu };

            if (fg.connect<"out">(ramp_down_source).to<"in">(ramp_down_to_pdu) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(ramp_down_to_pdu).to<"in">(ramp_down_pack) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(ramp_down_pack).to<"in">(ramp_down_modulator) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect(ramp_down_modulator, { "out" }, symbols_mux, { "in", 2 }) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }

            if (fg.connect(rrc_flush_source, { "out" }, symbols_mux, { "in", 3 }) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
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

        if (!stream_mode) {
            auto& rrc_interp =
                fg.emplaceBlock<InterpolatingFirFilter<Pdu<c64>, Pdu<c64>, float>>(
                    { { "interpolation", samples_per_symbol }, { "taps", rrc_taps } });
            if (max_in_samples) {
                rrc_interp.in.max_samples = max_in_samples;
            }
            if (out_buff_size) {
                if (rrc_interp.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }
            // burst shaper
            const size_t ramp_symbols = 4U;
            const size_t ramp_samples = ramp_symbols * samples_per_symbol;
            // offset to compensate group delay of RRC filter
            const size_t offset = 4U * samples_per_symbol;
            std::vector<float> leading_ramp(offset + ramp_samples);
            for (size_t j = 0; j < leading_ramp.size(); ++j) {
                leading_ramp[j] = static_cast<float>(std::sin(
                    static_cast<double>(j + 1) /
                    static_cast<double>(leading_ramp.size()) * 0.5 * std::numbers::pi));
            }
            std::vector<float> trailing_ramp(rrc_flush_nsymbols * samples_per_symbol -
                                             offset + ramp_samples);
            for (size_t j = 0; j < trailing_ramp.size(); ++j) {
                trailing_ramp[trailing_ramp.size() - 1 - j] = static_cast<float>(std::sin(
                    static_cast<double>(j + 1) /
                    static_cast<double>(trailing_ramp.size()) * 0.5 * std::numbers::pi));
            }
            auto& burst_shaper = fg.emplaceBlock<BurstShaper<Pdu<c64>, Pdu<c64>, float>>(
                { { "leading_shape", leading_ramp },
                  { "trailing_shape", trailing_ramp } });
            if (max_in_samples) {
                burst_shaper.in.max_samples = max_in_samples;
            }
            if (out_buff_size) {
                if (burst_shaper.out.resizeBuffer(out_buff_size) !=
                    ConnectionResult::SUCCESS) {
                    throw gr::exception("resizeBuffer() failed");
                }
            }
            if (fg.connect<"out">(symbols_mux).to<"in">(rrc_interp) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(rrc_interp).to<"in">(burst_shaper) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            out_packet = &burst_shaper.out;
        } else {
            // do not produce tags in PduToTaggedStream
            auto& pdu_to_stream =
                fg.emplaceBlock<PduToTaggedStream<c64>>({ { "packet_len_tag_key", "" } });
            if (max_in_samples) {
                pdu_to_stream.in.max_samples = max_in_samples;
            }
            out_count = &pdu_to_stream.count;
            auto& rrc_interp = fg.emplaceBlock<InterpolatingFirFilter<c64, c64, float>>(
                { { "interpolation", samples_per_symbol }, { "taps", rrc_taps } });
            if (fg.connect<"out">(symbols_mux).to<"in">(pdu_to_stream) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            if (fg.connect<"out">(pdu_to_stream).to<"in">(rrc_interp) !=
                ConnectionResult::SUCCESS) {
                throw std::runtime_error(connection_error);
            }
            out_stream = &rrc_interp.out;
        }

        if (fg.connect<"out">(ingress).to<"in">(crc_append) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"metadata">(ingress).to<"metadata">(header_formatter) !=
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
    }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PACKET_TRANSMITTER_PDU
