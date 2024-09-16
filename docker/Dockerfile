# syntax=docker/dockerfile:1

FROM ghcr.io/fair-acc/gr4-build-container:latest

ENV RUSTUP_HOME=/opt/rust
ARG CARGO_HOME=/opt/rust
ENV CC=clang-18
ENV CXX=clang++-18
USER 0

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path && \
    ln -sf /usr/bin/gcc /usr/local/bin/cc && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    libzmq3-dev libsoapysdr-dev soapysdr-module-all soapysdr-tools \
    python3-dev pybind11-dev python3-numpy meson libfmt9 libfmt-dev \
    && \
    git clone https://github.com/gnuradio/pmt/ && \
    cd pmt && \
    mkdir build && \
    meson wrap install gtest && \
    meson setup build && \
    cd build && \
    ninja && \
    ninja install && \
    mv /usr/local/lib/python3/dist-packages/pmtv /usr/local/lib/python3.12/dist-packages/ && \
    cd ../.. && \
    rm -rf pmt \
    rm -rf /var/lib/apt/lists/*

ENV PATH=/opt/rust/bin:$PATH

USER user
