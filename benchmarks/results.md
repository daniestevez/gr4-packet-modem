# Benchmark results

## Test system

These benchmark results have been obtained on a machine with an AMD Ryzen 7
5800X 8-core CPU and 16 GiB of RAM. The benchmarks have been run inside a
`gr4-packet-modem` Docker container. The host operating system is Arch Linux.

The `gr4-packet-modem` benchmarks have been built using the default environment
variables in the `gr4-packet-modem`. This uses clang 18.1.3 with no `-march`
flag, except for `fftw`, which is built with `-march=native`. A separate build
has been done with `-march=native` for all the software.

## Measurement procedure

All the benchmarks run continuously and have a Probe Rate block that measures
the sample rate. The benchmark is run for around 10-20 seconds and the
`rate_avg` from the Probe Rate block is taken. The benchmark is run several
time, and the set of sample rates obtained in all the runs are shown as an
interval in the results.

## Packet transmitter benchmark

The `benchmark_packet_transmitter` has been built against the
[tags-consume](ttps://github.com/daniestevez/gnuradio4/tree/tags-consume) branch
of gnuradio4.

| **Benchmark**                      | **burst mode** | **burst mode** `-march=native` | **stream mode** | **stream mode** `-march=native` |
|------------------------------------|----------------|--------------------------------|-----------------|---------------------------------|
| `benchmark_packet_transmitter_pdu` | 156-167 Msps   | 172-180 Msps                   | 65-72 Msps      | 100-125 Msps                    |
| `benchmark_packet_transmitter`     | 92-106 Msps    | 102-109 Msps                   | 100-106 Msps    | 65-67 Msps                      |

## Syncword detection benchmark

| **Number of frequency bins** |            | `-march=native` |
|------------------------------|------------|-----------------|
| 0                            | 49-51 Msps | 50-52 Msps      |
| 1                            | 29 Msps    | 30 Msps         |
| 2                            | 20-21 Msps | 20-22 Msps      |
| 3                            | 16 Msps    | 17 Msps         |
| 4                            | 13 Msps    | 13-14 Msps      |

## Packet receiver

| **Syncword frequency bins** |            | `-march=native` |
|-----------------------------|------------|-----------------|
| 0                           | 28-32 Msps | 25-32 Msps      |
| 1                           | 16-18 Msps | 15-19 Msps      |
| 2                           | 10-13 Msps | 11-14 Msps      |
| 3                           | 8-10 Msps  | 8-10 Msps       |
| 4                           | 6-8 Msps   | 6-8 Msps        |

## Packet transceiver

Some of these configurations are reported as 0 Msps because the flowgraph stops
passing samples after it has been running after a few seconds. The `rate_now`
reported by Probe Rate after that moment is zero. These cases run without
failing with the single-treaded scheduler.

| **Syncword frequency bins** | **packet mode** | **packet mode** `-march=native` | **stream mode** | **stream mode** `-march=native` |
|-----------------------------|-----------------|---------------------------------|-----------------|---------------------------------|
| 0                           | 25-27 Msps      | 24-29 Msps                      | 22-27 Msps      | 23-28 Msps                      |
| 1                           | 13-16 Msps      | 15-16 Msps                      | 12-14 Msps      | 12-14 Msps                      |
| 2                           | 10-11 Msps      | 10-11 Msps                      | 8-10 Msps       | 9-10 Msps                       |
| 3                           | 0 Msps (stops)  | 0 Msps (stops)                  | 6-8 Msps        | 7-8 Msps                        |
| 4                           | 0 Msps (stops)  | 0 Msps (stops)                  | 5-6 Msps        | 5.5-6.5 Msps                    |
