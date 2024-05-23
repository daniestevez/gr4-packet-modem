#ifndef _GR4_PACKET_MODEM_PACKET_TRANSMITTER
#define _GR4_PACKET_MODEM_PACKET_TRANSMITTER

#include <gnuradio-4.0/Graph.hpp>
#include <gnuradio-4.0/Scheduler.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/firdes.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/mapper.hpp>
#include <gnuradio-4.0/packet-modem/pack_bits.hpp>
#include <gnuradio-4.0/packet-modem/packet_ingress.hpp>
#include <gnuradio-4.0/packet-modem/packet_mux.hpp>
#include <gnuradio-4.0/packet-modem/pdu_to_tagged_stream.hpp>
#include <gnuradio-4.0/packet-modem/tagged_stream_to_pdu.hpp>
#include <gnuradio-4.0/packet-modem/unpack_bits.hpp>
#include <gnuradio-4.0/packet-modem/vector_source.hpp>
#include <cstdint>

namespace gr::packet_modem {

class PacketTransmitter
{
public:
    gr::PortIn<uint8_t>* in;
    gr::PortOut<std::complex<float>>* out;

    PacketTransmitter(gr::Graph& fg,
                      size_t samples_per_symbol = 4U,
                      const std::string& packet_len_tag_key = "packet_len")
    {
        using c64 = std::complex<float>;

        auto& ingress = fg.emplaceBlock<PacketIngress<>>(packet_len_tag_key);
        in = &ingress.in;

        // header
        auto& header_formatter = fg.emplaceBlock<HeaderFormatter>(packet_len_tag_key);
        // TODO: add header FEC encoder here

        // payload
        auto& crc_append = fg.emplaceBlock<CrcAppend<>>(32U,
                                                        0x4C11DB7U,
                                                        0xFFFFFFFFU,
                                                        0xFFFFFFFFU,
                                                        true,
                                                        true,
                                                        false,
                                                        0U,
                                                        packet_len_tag_key);
        // a payload FEC encoder block would be instantiated here

        // TODO: replace by stream PacketMux
        auto& header_to_pdu =
            fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>(packet_len_tag_key);
        header_to_pdu.name = "PacketTransmitter(header_to_pdu)";
        auto& payload_to_pdu =
            fg.emplaceBlock<TaggedStreamToPdu<uint8_t>>(packet_len_tag_key);
        payload_to_pdu.name = "PacketTransmitter(payload_to_pdu)";
        auto& header_payload_mux = fg.emplaceBlock<PacketMux<Pdu<uint8_t>>>(2U);
        header_payload_mux.name = "PacketTransmitter(header_payload_mux)";
        auto& muxed_to_stream =
            fg.emplaceBlock<PduToTaggedStream<uint8_t>>(packet_len_tag_key);
        muxed_to_stream.name = "PacketTransmitter(muxed_to_stream)";

        // TODO: evaluate replacement by a better scrambler (PN9 for instance)
        auto& scrambler_unpack =
            fg.emplaceBlock<UnpackBits<>>(8U, uint8_t{ 1 }, packet_len_tag_key);
        auto& scrambler = fg.emplaceBlock<AdditiveScrambler<uint8_t>>(
            0xA9U, 0xFFU, 7U, 0U, packet_len_tag_key);
        const float a = std::sqrt(2.0f) / 2.0f;
        const std::vector<c64> qpsk_constellation = {
            { a, a }, { a, -a }, { -a, a }, { -a, -a }
        };
        auto& qpsk_pack =
            fg.emplaceBlock<PackBits<>>(2U, uint8_t{ 1 }, packet_len_tag_key);
        auto& qpsk_modulator = fg.emplaceBlock<Mapper<uint8_t, c64>>(qpsk_constellation);

        // syncword (64-bit CCSDS syncword)
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
        const std::vector<Tag> syncword_tags = {
            { 0, { { packet_len_tag_key, syncword.size() } } }
        };
        auto& syncword_source =
            fg.emplaceBlock<VectorSource<uint8_t>>(syncword, true, syncword_tags);
        syncword_source.name = "PacketTransmitter(syncword_source)";
        const std::vector<c64> bpsk_constellation = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };
        auto& syncword_bpsk_modulator =
            fg.emplaceBlock<Mapper<uint8_t, c64>>(bpsk_constellation);

        // TODO: replace by stream PacketMux
        auto& syncword_to_pdu =
            fg.emplaceBlock<TaggedStreamToPdu<c64>>(packet_len_tag_key);
        syncword_to_pdu.name = "PacketTransmitter(syncword_to_pdu)";
        auto& payload_symbols_to_pdu =
            fg.emplaceBlock<TaggedStreamToPdu<c64>>(packet_len_tag_key);
        payload_symbols_to_pdu.name = "PacketTransmitter(payload_symbols_to_pdu)";
        auto& symbols_mux = fg.emplaceBlock<PacketMux<Pdu<c64>>>(2U);
        symbols_mux.name = "PacketTransmitter(symbols_mux)";
        auto& symbols_to_stream =
            fg.emplaceBlock<PduToTaggedStream<c64>>(packet_len_tag_key);
        symbols_to_stream.name = "PacketTransmitter(symbols_to_stream)";

        const size_t ntaps = samples_per_symbol * 11U;
        const auto rrc_taps = firdes::root_raised_cosine(
            1.0, static_cast<double>(samples_per_symbol), 1.0, 0.35, ntaps);
        auto& rrc_interp = fg.emplaceBlock<InterpolatingFirFilter<c64, c64, float>>(
            samples_per_symbol, rrc_taps);
        out = &rrc_interp.out;

        constexpr auto connection_error = "connection_error";

        if (fg.connect<"out">(ingress).to<"in">(crc_append) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (ingress.metadata.connect(header_formatter.metadata) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(header_formatter).to<"in">(header_to_pdu) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        // payload FEC connection would go here
        if (fg.connect<"out">(crc_append).to<"in">(payload_to_pdu) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(header_to_pdu, { "out" }, header_payload_mux, { "in", 0 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(payload_to_pdu, { "out" }, header_payload_mux, { "in", 1 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(header_payload_mux).to<"in">(muxed_to_stream) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(muxed_to_stream).to<"in">(scrambler_unpack) !=
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
        if (fg.connect<"out">(syncword_bpsk_modulator).to<"in">(syncword_to_pdu) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(qpsk_modulator).to<"in">(payload_symbols_to_pdu) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(syncword_to_pdu, { "out" }, symbols_mux, { "in", 0 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect(payload_symbols_to_pdu, { "out" }, symbols_mux, { "in", 1 }) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(symbols_mux).to<"in">(symbols_to_stream) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
        if (fg.connect<"out">(symbols_to_stream).to<"in">(rrc_interp) !=
            ConnectionResult::SUCCESS) {
            throw std::runtime_error(connection_error);
        }
    }
};

} // namespace gr::packet_modem

#endif // _GR4_PACKET_MODEM_PACKET_TRANSMITTER