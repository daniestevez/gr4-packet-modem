# gr4-packet-modem

gr4-packet-modem is a packet-based modem application that is being implemented
from scratch for GNU Radio 4.0. The goal of this project is twofold: it serves
as a use case to test and benchmark the GNU Radio 4.0 runtime in a realistic
digital communications application, and it provides an example of a full system
that users will be able to study and customize to their needs. The modem uses
QPSK with RRC pulse-shape, but there are provisions for supporting additional
MODCODs. It can work either in burst mode, where a packet is only transmitted
when there is data, or in stream mode, where packets are transmitted
back-to-back and idle packets are inserted when necessary. The modem can be used
for IP communications with a TUN device in Linux. The receiver uses some
advanced techniques, such as symbol time and carrier frequency and phase
estimation with FFT-based syncword correlation.

## Quick start

The following steps run a flowgraph that connects the modem TX to the modem RX
through a channel model and provides IP communications through the modem on a
single machine using network namespaces. Each of the steps is explained in more
details in the rest of the documentation.

In a `gr4-packet-modem` checkout, run the following to set up the network
namespaces and interfaces (see the [network namespaces
documentation](docs/netns.md) for more details).

```
scripts/netns-setup
```

Run a Docker image that contains `gr4-packet-modem` already built.

```
docker run --rm --net host --user=0 \
    --cap-add=NET_ADMIN --cap-add=SYS_ADMIN \
    --device /dev/net/tun -v /var/run/netns:/var/run/netns \
    -it ghcr.io/daniestevez/gr4-packet-modem-built
```

Start the flowgraph in this root shell in the Docker container.

```
/home/user/gr4-packet-modem/build/apps/packet_transceiver 20.0 0.005 1.2 0
```

The arguments indicate an Es/N0 of 20 dB, a carrier frequency error of 0.005
rad/sample, a sampling frequency offset of 1.2 ppm, and burst TX mode. The
flowgraph runs at a sample rate of 3.2 Msps, which gives 800 ksyms at 4
samples/symbol.

Optionally, in the `gr4-packet-modem` checkout in the host run the following to
get a Python GUI that displays constellation plots in real time.

```
scripts/plot_symbols.py
```

Start a ping, by running the following in the host.

```
sudo ip netns exec gr4_tx ping 192.168.10.2
```

Run a TCP `iperf3` test by running the following two commands in separate shells in the host.

```
sudo ip netns exec gr4_rx iperf3 -s
```

```
sudo ip netns exec gr4_tx iperf3 -c 192.168.10.2
```

Using uncoded QPSK at 800 ksyms, the maximum data rate is 1.6 Mbps. There is
some overhead due to headers and silent gaps between some packets, so a data
rate of around 1.4 Mbps is expected in `iperf3`.

## Docker images

There are two Docker images that can be useful to develop and run gr4-packet-modem:

- [ghcr.io/daniestevez/gr4-packet-modem](https://github.com/users/daniestevez/packages/container/package/gr4-packet-modem). This
  image is intended for building, developing and running gr4-packet-modem. It
  contains all the dependencies needed for building and running the software. It
  is based on
  [ghcr.io/fair-acc/gr4-build-container](https://github.com/orgs/fair-acc/packages/container/package/gr4-build-container),
  but it contains additional software.

- [ghcr.io/daniestevez/gr4-packet-modem-built](https://github.com/users/daniestevez/packages/container/package/gr4-packet-modem-built). This
  image is based on ghcr.io/daniestevez/gr4-packet-modem and it contains a
  pre-built gr4-packet-modem in `/home/user/gr4-packet-modem/build`. It is
  intended to be used for running gr4-packet-modem, specially in low-end
  machines where building the software can take a long time. This image is built
  using Github actions, so the `:latest` tag always corresponds to the latest
  gr4-packet-modem main branch.

The Dockerfiles used to build these images are in the [docker](docker)
directory. They are, respectively, [`Dockerfile`](docker/Dockerfile) and
[`Dockerfile.built`](docker/Dockerfile.built). The context for building
`Dockefile.built` needs to be the git checkout directory for gr4-packet-modem,
since the whole checkout is added to the image using `ADD`.

See [Docker image usage](docs/docker.md) for instructions about how to run these
docker images.

## Building

gr4-packet-modem is built with CMake. It can be built from a git checkout as follows:

```
mkdir build
cd build
cmake ..
make -j$(nproc)
```

Depending on the CPU count and RAM of the machine, there might not be enough RAM
to build with `$(nproc)` processes in parallel, so the `-j` argument of `make`
might need to be adjusted.

Most of the blocks have some prints for trace-like debugging that are gated
under `#ifdef TRACE`. Trace printing can be enabled by running `cmake` like so:

```
cmake -D CMAKE_CXX_FLAGS=-DTRACE ..
make -j$(nproc)
```

## Repository organization

This repository is organized in directories in the following way.

* [apps](apps). This contains several applications, each of which is a flowgraph
  that gets built to an executable. The applications contain either the TX or
  the RX part of the modem, or both, in different configurations.

* [blocks](blocks). This contains all the GNU Radio 4.0 blocks implemented in
  this repository. All the blocks are `.hpp` headers, with one file per block
  (and another supporting headers which do not define blocks).

* [examples](examples). This contains example flowgraphs. Typically, the
  examples test or showcase how to use a particular block or technique, without
  being structured as a proper QA test. Each example is a flowgraph that gets
  built to an executable.

* gnuradio4. This is the [gnuradio4](https://github.com/fair-acc/gnuradio4)
  repository as a submodule.

* [gr3](gr3). This contains supporting files from GNU Radio 3.10. Currently the
  only thing in this directory is [gr3/flowgraphs](gr3/flowgraphs), which
  contains some GNU Radio 3.10 flowgraphs that can be used in combination with
  gr4-packet-modem.

* [scripts](scripts). Contains some utility scripts.

* [test](test). Contains QA tests for most of the blocks in the
  repository. Generally, each QA test puts together and runs a flowgraph where
  the functionality of a particular block is tested. As in gnuradio4, the tests
  use [boost/ut](https://github.com/boost-ext/ut) and are run usting `ctest`.

