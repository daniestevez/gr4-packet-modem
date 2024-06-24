# syntax=docker/dockerfile:1

FROM ghcr.io/fair-acc/gr4-build-container:latest

ENV RUSTUP_HOME=/opt/rust
ARG CARGO_HOME=/opt/rust
USER 0

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --no-modify-path && \
    ln -sf /usr/bin/gcc /usr/local/bin/cc && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y libzmq3-dev && \
    rm -rf /var/lib/apt/lists/*

USER user
