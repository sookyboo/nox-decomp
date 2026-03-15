# syntax=docker/dockerfile:1.7

FROM ubuntu:20.04 AS base
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /build

RUN sed -i 's|http://archive.ubuntu.com/ubuntu|https://archive.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
 && sed -i 's|http://security.ubuntu.com/ubuntu|https://security.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      ca-certificates \
      wget \
      git \
      cmake \
      ninja-build \
      build-essential \
      pkg-config \
      file \
      xz-utils \
      unzip \
      mingw-w64 \
      g++-aarch64-linux-gnu \
      gcc-aarch64-linux-gnu \
      binutils-aarch64-linux-gnu \
      liblzma-dev \
      zlib1g-dev \
      libbz2-dev \
      libboost-program-options-dev \
      libboost-iostreams-dev \
      libboost-filesystem-dev \
      libboost-system-dev \
      libboost-date-time-dev \
 && rm -rf /var/lib/apt/lists/*

ARG INNOEXTRACT_VERSION=1.9
RUN git clone --branch ${INNOEXTRACT_VERSION} --depth 1 https://github.com/dscharrer/innoextract.git /build/innoextract \
 && sed -i 's/^cmake_minimum_required(VERSION 2\\.8)$/cmake_minimum_required(VERSION 3.5)/' /build/innoextract/cmake/VersionScript.cmake

# ------------------------------
# Linux x86_64
# ------------------------------
FROM base AS linux-amd64
RUN cmake -S /build/innoextract -B /build/out-linux-amd64 -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/dist \
 && cmake --build /build/out-linux-amd64 -j"$(nproc)" \
 && cmake --install /build/out-linux-amd64 \
 && mkdir -p /artifact \
 && cp -av /dist/bin/innoextract /artifact/innoextract-linux-x86_64 \
 && file /artifact/innoextract-linux-x86_64

# ------------------------------
# Windows x86_64
# ------------------------------
FROM base AS windows-amd64
RUN cmake -S /build/innoextract -B /build/out-win-amd64 -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
      -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
      -DCMAKE_INSTALL_PREFIX=/dist \
 && cmake --build /build/out-win-amd64 -j"$(nproc)" \
 && cmake --install /build/out-win-amd64 \
 && mkdir -p /artifact \
 && cp -av /dist/bin/innoextract.exe /artifact/innoextract-win64.exe \
 && file /artifact/innoextract-win64.exe

# ------------------------------
# Linux aarch64
# Best built with buildx --platform=linux/arm64
# This stage assumes the container itself is arm64 when invoked that way.
# ------------------------------
FROM base AS linux-arm64
RUN cmake -S /build/innoextract -B /build/out-linux-arm64 -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/dist \
 && cmake --build /build/out-linux-arm64 -j"$(nproc)" \
 && cmake --install /build/out-linux-arm64 \
 && mkdir -p /artifact \
 && cp -av /dist/bin/innoextract /artifact/innoextract-linux-aarch64 \
 && file /artifact/innoextract-linux-aarch64