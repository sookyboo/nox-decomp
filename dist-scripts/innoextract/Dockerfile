# syntax=docker/dockerfile:1.7

FROM ubuntu:20.04 AS base
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /build

# Bootstrap certs over HTTP first
RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates \
 && sed -i 's|http://archive.ubuntu.com/ubuntu|https://archive.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
 && sed -i 's|http://security.ubuntu.com/ubuntu|https://security.ubuntu.com/ubuntu|g' /etc/apt/sources.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
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
      autoconf \
      automake \
      libtool \
      gettext \
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
      -DUSE_STATIC_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/dist \
 && cmake --build /build/out-linux-amd64 -j"$(nproc)" \
 && cmake --install /build/out-linux-amd64 \
 && mkdir -p /artifact \
 && cp -av /dist/bin/innoextract /artifact/innoextract-linux-x86_64 \
 && file /artifact/innoextract-linux-x86_64 \
 && ldd /artifact/innoextract-linux-x86_64 || true

# ------------------------------
# Windows x86_64
# ------------------------------
# Build liblzma for Windows target
FROM base AS mingw-lzma
ARG XZ_VERSION=5.4.6
RUN wget -O /tmp/xz.tar.xz https://tukaani.org/xz/xz-${XZ_VERSION}.tar.xz \
 && mkdir -p /tmp/xz-src \
 && tar -xf /tmp/xz.tar.xz -C /tmp/xz-src --strip-components=1 \
 && cd /tmp/xz-src \
 && ./configure \
      --host=x86_64-w64-mingw32 \
      --prefix=/opt/mingw-xz \
      --disable-shared \
      --enable-static \
 && make -j"$(nproc)" \
 && make install


FROM base AS mingw-zlib
ARG ZLIB_VERSION=1.3.1
RUN wget -O /tmp/zlib.tar.gz https://zlib.net/fossils/zlib-${ZLIB_VERSION}.tar.gz \
 && mkdir -p /tmp/zlib-src \
 && tar -xzf /tmp/zlib.tar.gz -C /tmp/zlib-src --strip-components=1 \
 && cd /tmp/zlib-src \
 && CC=x86_64-w64-mingw32-gcc \
    AR=x86_64-w64-mingw32-ar \
    RANLIB=x86_64-w64-mingw32-ranlib \
    ./configure --static --prefix=/opt/mingw-zlib \
 && make -j"$(nproc)" \
 && make install

FROM base AS mingw-bzip2
ARG BZIP2_VERSION=1.0.8
RUN wget -O /tmp/bzip2.tar.gz https://sourceware.org/pub/bzip2/bzip2-${BZIP2_VERSION}.tar.gz \
 && mkdir -p /tmp/bzip2-src \
 && tar -xzf /tmp/bzip2.tar.gz -C /tmp/bzip2-src --strip-components=1 \
 && cd /tmp/bzip2-src \
 && make -f Makefile-libbz2_so clean || true \
 && make -j"$(nproc)" \
      CC=x86_64-w64-mingw32-gcc \
      AR=x86_64-w64-mingw32-ar \
      RANLIB=x86_64-w64-mingw32-ranlib \
      bzip2 bzip2recover libbz2.a \
 && mkdir -p /opt/mingw-bzip2/include /opt/mingw-bzip2/lib \
 && cp -av bzlib.h /opt/mingw-bzip2/include/ \
 && cp -av libbz2.a /opt/mingw-bzip2/lib/


FROM base AS mingw-boost
ARG BOOST_VERSION=1.84.0
ARG BOOST_VERSION_U=1_84_0

COPY --from=mingw-zlib  /opt/mingw-zlib  /opt/mingw-zlib
COPY --from=mingw-bzip2 /opt/mingw-bzip2 /opt/mingw-bzip2

RUN wget -O /tmp/boost.tar.gz \
      https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_U}.tar.gz \
 && mkdir -p /tmp/boost-src \
 && tar -xzf /tmp/boost.tar.gz -C /tmp/boost-src --strip-components=1 \
 && cd /tmp/boost-src \
 && printf '%s\n' 'using gcc : mingw : x86_64-w64-mingw32-g++ ;' > user-config.jam \
 && printf '%s\n' 'using zlib : 1.3.1 : <include>/opt/mingw-zlib/include <search>/opt/mingw-zlib/lib ;' >> user-config.jam \
 && printf '%s\n' 'using bzip2 : 1.0.8 : <include>/opt/mingw-bzip2/include <search>/opt/mingw-bzip2/lib ;' >> user-config.jam \
 && ./bootstrap.sh \
      --prefix=/opt/mingw-boost \
      --with-libraries=filesystem,iostreams,date_time,system,program_options,regex \
 && ./b2 \
      -j"$(nproc)" \
      toolset=gcc-mingw \
      target-os=windows \
      address-model=64 \
      architecture=x86 \
      threading=multi \
      link=static \
      runtime-link=static \
      variant=release \
      --layout=system \
      --user-config=user-config.jam \
      --prefix=/opt/mingw-boost \
      install

FROM base AS windows-amd64
COPY --from=mingw-lzma  /opt/mingw-xz    /opt/mingw-xz
COPY --from=mingw-zlib  /opt/mingw-zlib  /opt/mingw-zlib
COPY --from=mingw-bzip2 /opt/mingw-bzip2 /opt/mingw-bzip2
COPY --from=mingw-boost /opt/mingw-boost /opt/mingw-boost

RUN cmake -S /build/innoextract -B /build/out-win-amd64 -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
      -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
      -DUSE_LTO=OFF \
      -DUSE_LD=bfd \
      -DLZMA_INCLUDE_DIR=/opt/mingw-xz/include \
      -DLZMA_LIBRARY=/opt/mingw-xz/lib/liblzma.a \
      -DZLIB_INCLUDE_DIR=/opt/mingw-zlib/include \
      -DZLIB_LIBRARY=/opt/mingw-zlib/lib/libz.a \
      -DBZIP2_INCLUDE_DIR=/opt/mingw-bzip2/include \
      -DBZIP2_LIBRARIES=/opt/mingw-bzip2/lib/libbz2.a \
      -DBOOST_ROOT=/opt/mingw-boost \
      -DBoost_NO_SYSTEM_PATHS=ON \
      -DBoost_USE_STATIC_LIBS=ON \
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
      -DUSE_STATIC_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/dist \
 && cmake --build /build/out-linux-arm64 -j"$(nproc)" \
 && cmake --install /build/out-linux-arm64 \
 && mkdir -p /artifact \
 && cp -av /dist/bin/innoextract /artifact/innoextract-linux-aarch64 \
 && file /artifact/innoextract-linux-aarch64 \
 && ldd /artifact/innoextract-linux-aarch64 || true

# ------------------------------
# Export-only stage
# ------------------------------
FROM scratch AS artifacts
COPY --from=linux-amd64  /artifact/ /linux-x86_64/
COPY --from=linux-arm64  /artifact/ /linux-aarch64/
COPY --from=windows-amd64 /artifact/ /windows-x86_64/