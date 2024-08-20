# gr4-packet-modem applications

This describes the usage of each of the applications in this directory. Most of
these applications need to be run as root, because they use network namespaces.

## packet_receiver_file

```
usage: packet_receiver_file input_file [syncword_freq_bins] [syncword_threshold]

the default syncword freq bins is 4
the default syncword threshold is 9.5
```

The `packet_receiver_file` application reads IQ samples from an input file in
raw `complex64` (`std::complex<float>`) format at 4 samples/symbol, runs the
modem receiver with these IQ samples and writes decoded IP packets to the
`gr4_tun_rx` TUN device in the `gr4_rx` namespace. Optionally, the syncword
frequency search range and the syncword threshold can be specified as
arguments. See `packet_receiver_soapy` for details on these arguments.

This application can be used to process a previously recorded IQ file, or to
read in real time from a FIFO in which an application such as
`packet_transmitter_sdr` is writing.

## packet_receiver_soapy

```
usage: packet_receiver_soapy rf_freq_hz [samp_rate_sps] [syncword_freq_bins] [syncword_threshold]

the default sample rate is 3.2 Msps
the default syncword freq bins is 4
the default syncword threshold is 9.5
```

The `packet_receiver_soapy` application reads IQ samples from an SDR using the
Soapy source block available in GNU Radio 4.0. The command line arguments are
the RF carrier frequency in Hz and optionally the sample rate in sps and the
syncword search range and detection threshold. The syncword frequency search
range is defined in terms of the `syncword_freq_bins` parameter. The frequency
bins searched are all from `-syncword_freq_bins` to `syncword_freq_bins`. The
syncword detection threshold is given in units of power. An appropriate syncword
threshold depends on the frequency search range, due to how the detection
condition works. The default of 9.5 is good for `syncword_freq_bins`, but lower
values of `syncword_freq_bins` should use a higher threshold.

The Soapy block is set to use an `rtlsdr` device with an RX gain of 30 dB, but
this can be changed in the source code
[`packet_receiver_soapy.cpp`](packet_receiver_soapy.cpp).

The receiver expects a signal at 4 samples/symbol. Received IP packets are
written to the `gr4_tun_rx` TUN device in the `gr4_rx` namespace.

This application can be used to receive modem packets using an SDR.

## packet_transceiver

```
usage: packet_transceiver esn0_db cfo_rad_samp sfo_ppm stream_mode [samp_rate_sps] [syncword_freq_bins] [syncword_threshold]

the default sample rate is 3.2 Msps
the default syncword freq bins is 4
the default syncword threshold is 9.5
```

The `packet_transceiver` application runs the modem transmitter and receiver in
the same GNU Radio 4.0 flowgraph. They are connected by a channel model that
generates AWGN to set a certain SNR and applies a carrier frequency offset and
sampling frequency offset. The command line arguments are the Es/N0 in dB, the
carrier frequency offset in radians/sample, the sampling frequency offset in
PPM, whether to use stream mode or burst mode, and optionally the sample rate to
which the flowgraph is throttled and the syncword frequency search range and
detection threshold (defined as in `packet_receiver_soapy`). The transceiver
uses 4 samples/symbol. A Probe Rate block periodically prints the sample rate,
as a quick way to verify if the CPU is able to keep up with the intended sample
rate.

The transmitter reads IP packets from the `gr4_tun_tx` TUN device in the
`gr4_tx` namespace, and the receiver writes decoded IP packets to the
`gr4_tun_rx` TUN device in the `gr4_rx` namespace.

This application can be used for an end-to-end test of the modem within a single
GNU Radio 4.0 flowgraph.

## packet_transmitter_pdu

```
usage: packet_transmitter_pdu output_file stream_mode
```

The `packet_transmitter_pdu` application contains the modem transmitter
(implemented using `gr::packet_modem::Pdu<>`-aware blocks) and writes IQ samples
to an output file. PDUs of size 1500 bytes containing only zeros are
continuously sent to the modem transmitter input. There is no throttle in this
flowgraph. It will write as fast as the output file can accept data or as fast
as the CPU can keep up. The application does not use TUN devices or network
namespaces, so it does not need to be run as root. The modem transmitter uses 4
samples/symbol. A Probe Rate block periodically prints the sample rate.

This application can be used as a simple flowgraph that contains the modem
transmitter. It can be used to benchmark the CPU performance of the transmitter.

## packet_transmitter_pdu_throttle

```
usage: packet_transmitter_pdu_throttle output_file samp_rate stream_mode
```

The `packet_transmitter_throttle` application contains the modem transmitter, a
throttle block, and a file output. Unlike `packet_transmitter_pdu` the PDUs sent
to the input of the modem transmitter are read from the `gr4_tun_tx` TUN device
in the `gr4_tx` namespace (but in the code there is the provision to send dummy
PDUs of fixed size continuously instead). The transmitter uses 4
samples/symbol. A Probe Rate block periodically prints the sample rate, as a
quick way to verify that the CPU can achieve the intended sample rate.

This application does not contain a latency management system, so in general it
should not be used for over-the-air IP communications (see
`packet_transmitter_sdr` instead). It can be used for miscellanous tests and
benchmarks, and as a way to verify the consequences of not including a latency
management system in the transmitter.

## packet_transmitter_sdr

```
usage: packet_transmitter_sdr output_file stream_mode
```

The `packet_transmitter_sdr` application is similar to `packet_transmitter_pdu`,
except that it does not contain a throttle block but it contains a latency
management system. Also, there is no Probe Rate block.

This application is intended to be used together with the GNU Radio 3.10
flowgraph in `gr3/flowgraphs/file_source_uhd.grc`. Such flowgraph reads IQ
samples from a file and sends them to a UHD Sink. The `packet_transmitter_sdr`
and this flowgraph can be connected with a FIFO (created using `mkfifo`). This
allows using the modem transmitter with a USRP (or any other SDR supported in
GNU Radio 3.10). At the moment there is no SDR transmitter support within GNU
Radio 4.0.
