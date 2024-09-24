#include <gnuradio-4.0/Block.hpp>
#include <gnuradio-4.0/packet-modem/add.hpp>
#include <gnuradio-4.0/packet-modem/additive_scrambler.hpp>
#include <gnuradio-4.0/packet-modem/binary_slicer.hpp>
#include <gnuradio-4.0/packet-modem/burst_shaper.hpp>
#include <gnuradio-4.0/packet-modem/coarse_frequency_correction.hpp>
#include <gnuradio-4.0/packet-modem/constellation_llr_decoder.hpp>
#include <gnuradio-4.0/packet-modem/costas_loop.hpp>
#include <gnuradio-4.0/packet-modem/crc_append.hpp>
#include <gnuradio-4.0/packet-modem/crc_check.hpp>
#include <gnuradio-4.0/packet-modem/file_sink.hpp>
#include <gnuradio-4.0/packet-modem/file_source.hpp>
#include <gnuradio-4.0/packet-modem/glfsr_source.hpp>
#include <gnuradio-4.0/packet-modem/head.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_decoder.hpp>
#include <gnuradio-4.0/packet-modem/header_fec_encoder.hpp>
#include <gnuradio-4.0/packet-modem/header_formatter.hpp>
#include <gnuradio-4.0/packet-modem/header_parser.hpp>
#include <gnuradio-4.0/packet-modem/header_payload_split.hpp>
#include <gnuradio-4.0/packet-modem/interpolating_fir_filter.hpp>
#include <gnuradio-4.0/packet-modem/null_sink.hpp>
#include <gnuradio-4.0/packet-modem/null_source.hpp>
#include <gnuradio-4.0/packet-modem/pdu.hpp>
#include <complex>
#include <cstdint>

#include "register_helpers.hpp"

void register_add();
void register_additive_scrambler();
void register_binary_slicer();
void register_burst_shaper();
void register_coarse_frequency_correction();
void register_constellation_llr_decoder();
void register_costas_loop();
void register_crc_append();
void register_crc_check();
void register_file_sink();
void register_file_source();
void register_glfsr_source();
void register_head();
void register_header_fec_decoder();
void register_header_fec_encoder();
void register_header_formatter();
void register_header_parser();
void register_header_payload_split();
void register_interpolating_fir_filter();
void register_item_strobe();
void register_mapper();
void register_message_debug();
void register_message_strobe();
void register_multiply_packet_len_tag();
void register_noise_source();
void register_null_sink();
void register_null_source();
void register_pack_bits();
void register_packet_counter();
void register_packet_ingress();
void register_packet_limiter();

void register_blocks()
{
    register_add();
    register_additive_scrambler();
    register_binary_slicer();
    register_burst_shaper();
    register_coarse_frequency_correction();
    register_constellation_llr_decoder();
    register_costas_loop();
    register_crc_append();
    register_crc_check();
    register_file_sink();
    register_file_source();
    register_glfsr_source();
    register_head();
    register_header_fec_decoder();
    register_header_fec_encoder();
    register_header_formatter();
    register_header_parser();
    register_header_payload_split();
    register_interpolating_fir_filter();
    register_item_strobe();
    register_mapper();
    register_message_debug();
    register_message_strobe();
    register_multiply_packet_len_tag();
    register_noise_source();
    register_null_sink();
    register_null_source();
    register_pack_bits();
    register_packet_counter();
    register_packet_ingress();
    register_packet_limiter();
}
