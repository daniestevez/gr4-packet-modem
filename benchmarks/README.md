# gr4-packet-modem benchmarks

This directory contains some benchmark flowgraphs for gr4-packet-modem. Each of
the flowgraphs runs at the maximum sample rate that the CPU can achieve (there
are no Throttle blocks or hardware limiting the sample rate) and has a Probe
Rate block that measures and reports the sample rate.

All the benchmarks use the multi-threaded simple scheduler
(`gr::scheduler::Simple<gr::scheduler::ExecutionPolicy::multiThreaded>`). This
scheduler spawns a worker thread for each CPU, pins each worker to a single CPU
using CPU affinities, and statically distributes all the blocks in the flowgraph
over the workers (if there are less blocks than CPUs, then only as many workers
as blocks are spawned). There are currently two potential issues with this scheduler:

- The worker threads busy wait when none of their blocks can be run. This busy
  waiting means that all the CPUs run at 100%. Besides power efficiency
  concerns, this also robs CPU clock boost from the CPUs that are doing useful
  work and could benefit from boosting. It also difficults identifying which
  blocks are the ones that use more CPU and thus constitute a bottleneck for the
  flowgraph.

- At the beginning of the flowgraph execution all the blocks are distributed to
  worker threads in a fixed deterministic order. It might happen that multiple
  blocks which are CPU intensive get assigned to the same worker, while other
  workers might only get blocks that use little CPU. In this situation, the
  one-thread-per-block approach used by the GNU Radio 3.10 scheduler can be better.

## Benchmark flowgraphs

### `benchmark_packet_transmitter_pdu`

This is a benchmark of the packet transmitter implemented using `Pdu<T>` data
types. Packets of 1500 bytes are sent continuously to the transmitter. They are
modulated at 4 samples/symbol. The output sample rate of the transmitter is
measured. The benchmark takes one parameter that indicates whether to use stream
mode or burst mode in the transmitter.

### `benchmark_packet_transmitter`

This is a variant of the packet transmitter benchmark that uses the packet
transmitter implemented using `"packet_len"` tags to delimit packet
boundaries. Its performance is bad due to some current performance issues of GNU
Radio 4.0 regarding tag processing. Currently this flowgraph aborts when built
against the gnuradio4 main branch due to an issue with tag consumption. It needs
to be built against the
[tags-consume](https://github.com/daniestevez/gnuradio4/tree/tags-consume)
branch instead.

### `benchmark_syncword_detection`

This benchmark measures the performance of the Syncword Detection block by
feeding it zeros from a Null Source. The number of frequency bins used by the
syncword detection algorithm and the syncword detection threshold are configurable.

### `benchmark_packet_receiver`

This benchmark connects a Null Source to the full packet receiver. It measures
the sample rate at which the packet receiver can ingest IQ data. The syncword
detection parameters are configurable, as in the `benchmark_syncword_detection`
flowgraph.

### `benchmark_packet_transceiver`

This benchmark contains a the blocks from `benchmark_tranmitter_pdu` connected
to the input of the packet receiver. It measures the IQ sample rate in the
connection between the transmitter and receiver. The configuration parameters
are those of `benchmark_transmitter_pdu` and those of `benchmark_packet_receiver`.

## Benchmark results

The results of running these benchmarks in a relatively modern AMD desktop CPU
are collected in [results.md](results.md).
