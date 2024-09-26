#!/usr/bin/env python3

import gr4_packet_modem
import numpy as np
import pmtv

import argparse
import math


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('--scheduler', default='SchedulerSimpleSingleThreaded')
    parser.add_argument('output_file')
    return parser.parse_args()


def main():
    args = parse_args()

    sched = getattr(gr4_packet_modem, args.scheduler)()
    fg = sched.graph()

    source = fg.emplaceBlock(
        'gr::packet_modem::RandomSource', 'unsigned char',
        {'minimum': gr4_packet_modem.pmt_from_uint8_t(0),
         'maximum': gr4_packet_modem.pmt_from_uint8_t(255),
         'num_items': gr4_packet_modem.pmt_from_size_t(1000000),
         'repeat': pmtv.pmt(True)})
    unpack = fg.emplaceBlock(
        'gr::packet_modem::UnpackBits', 'MSB,unsigned char,unsigned char',
        {'outputs_per_input': gr4_packet_modem.pmt_from_size_t(4),
         'bits_per_output': gr4_packet_modem.pmt_from_uint8_t(2)})
    # TODO: why std::complex<T> ??? This is a problem with how the block types
    # are registered
    constellation_modulator = fg.emplaceBlock(
        'gr::packet_modem::Mapper',
        'unsigned char,std::complex<T>', {})
    a = math.sqrt(2) / 2
    qpsk_constellation = [a + 1j*a, a - 1j*a, -a + 1j*a, -a - 1j*a]
    gr4_packet_modem.set_mapper_u8_c64_map(
        constellation_modulator, qpsk_constellation)
    sps = 4
    ntaps = sps * 11
    rrc_taps = gr4_packet_modem.root_raised_cosine(1.0, sps, 1.0, 0.35, ntaps)
    rrc_interp = fg.emplaceBlock(
        "gr::packet_modem::InterpolatingFirFilter",
        'std::complex<T>,std::complex<T>,float',
        {'interpolation': gr4_packet_modem.pmt_from_size_t(sps),
         # note that a float32 np.array is needed here because otherwise
         # pmtv.pmt() yields a pmt of doubles instead of floats
         'taps': pmtv.pmt(np.array(rrc_taps, 'float32'))})
    sink = fg.emplaceBlock(
        'gr::packet_modem::FileSink', 'std::complex<T>',
        {'filename': pmtv.pmt(args.output_file)})
    probe_rate = fg.emplaceBlock(
        'gr::packet_modem::ProbeRate', 'std::complex<T>', {})
    message_debug = fg.emplaceBlock('gr::packet_modem::MessageDebug', '', {})

    fg.connect(source, 'out', unpack, 'in')
    fg.connect(unpack, 'out', constellation_modulator, 'in')
    fg.connect(constellation_modulator, 'out', rrc_interp, 'in')
    fg.connect(rrc_interp, 'out', sink, 'in')
    fg.connect(rrc_interp, 'out', probe_rate, 'in')
    fg.connect(probe_rate, 'rate', message_debug, 'print')

    sched.runAndWait()


if __name__ == '__main__':
    main()
