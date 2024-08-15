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

