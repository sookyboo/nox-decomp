FROM ubuntu:20.04

# Run with: docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile -t noxdecomp-build . && docker create --name noxdecomp_tmp noxdecomp-build && docker cp noxdecomp_tmp:/build/nox-decomp/build/src/out ./noxd.armhf && docker cp noxdecomp_tmp:/build/gl4es/lib/libGL.so.1 libGL.so.1 && docker rm noxdecomp_tmp
#
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /build

# -------------------------------------------------
# Enable armhf architecture + toolchain + armhf sysroot headers/libs
# -------------------------------------------------
RUN dpkg --add-architecture armhf && \
    apt-get update && apt-get install -y \
      build-essential \
      ccache \
      cmake \
      ninja-build \
      git \
      pkg-config \
      wget \
      ca-certificates \
      autoconf \
      automake \
      libtool \
      python3 \
      \
      gcc-arm-linux-gnueabihf \
      g++-arm-linux-gnueabihf \
      binutils-arm-linux-gnueabihf \
      \
      libc6-dev-armhf-cross \
      libstdc++-9-dev-armhf-cross \
      linux-libc-dev-armhf-cross \
      \
      libsdl2-dev:armhf \
      libz-dev:armhf \
      libpng-dev:armhf \
      libbz2-dev:armhf \
      libssl-dev:armhf \
      liblua5.1-0-dev:armhf \
      libasound2-dev:armhf \
      libogg-dev:armhf \
      libvorbis-dev:armhf \
      libtheora-dev:armhf \
      libopenal-dev:armhf \
      \
      make \
      yasm \
      nasm \
      gettext \
      texinfo \
      \
      zlib1g-dev:armhf \
      libbz2-dev:armhf \
      libssl-dev:armhf \
      \
      innoextract \
      zip && \
    rm -rf /var/lib/apt/lists/*


# Ensure pkg-config resolves ARMHF (prevents host/aarch64 contamination)
ENV PKG_CONFIG_PATH=/opt/ffmpeg-armhf/lib/pkgconfig:/usr/lib/arm-linux-gnueabihf/pkgconfig
ENV PKG_CONFIG_LIBDIR=/opt/ffmpeg-armhf/lib/pkgconfig:/usr/lib/arm-linux-gnueabihf/pkgconfig
ENV PKG_CONFIG_SYSROOT_DIR=

# -------------------------------------------------
# Toolchain file (Ubuntu multiarch style; no CMAKE_SYSROOT)
# -------------------------------------------------
RUN cat <<'EOF' > /toolchain-armhf.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

set(CMAKE_FIND_ROOT_PATH
    /usr/arm-linux-gnueabihf
    /usr/lib/arm-linux-gnueabihf
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# gl4es lib

WORKDIR /build
RUN git clone https://github.com/ptitSeb/gl4es.git

RUN apt-get update && apt-get install -y \
          build-essential \
          ccache \
          cmake \
          ninja-build \
          git \
          pkg-config \
          wget \
          ca-certificates \
          libegl1-mesa-dev:armhf \
          libgles2-mesa-dev:armhf \
          libgbm-dev:armhf \
          libdrm-dev:armhf \
          mesa-common-dev:armhf \
          \
          libx11-dev:armhf \
          libxext-dev:armhf \
          libxfixes-dev:armhf \
          libxdamage-dev:armhf \
          libxxf86vm-dev:armhf

# gl4es: fix CMake module + function usage (CMake 3.16 compatible)
RUN sed -i \
    -e 's/include(CheckCompilerFlag)/include(CheckCCompilerFlag)/' \
    -e 's/check_compiler_flag(C /check_c_compiler_flag(/g' \
    /build/gl4es/CMakeLists.txt

# Sanity check
RUN grep -n "CheckCCompilerFlag" /build/gl4es/CMakeLists.txt && \
    grep -n "check_c_compiler_flag" /build/gl4es/CMakeLists.txt

RUN cd gl4es && mkdir -p build && cd build && \
    cmake .. \
      -DCMAKE_TOOLCHAIN_FILE=/toolchain-armhf.cmake \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DNOX11=ON -DGLX_STUBS=ON -DEGL_WRAPPER=ON -DGBM=ON \
      -DCMAKE_INSTALL_PREFIX=/usr

RUN cd gl4es/build && \
    make -j"$(nproc)"

RUN file /build/gl4es-armhf/usr/lib*/libGL.so* || true

# ffmpeg for videos

# -------------------------------------------------
# Build FFmpeg (armhf) from git with VQA3 support
# -------------------------------------------------
ARG FFMPEG_GIT_REF=n7.1.1
# You can switch to "master" if you want latest:
# ARG FFMPEG_GIT_REF=master


ARG FFMPEG_PREFIX=/opt/ffmpeg-armhf
ENV FFMPEG_PREFIX=${FFMPEG_PREFIX}

WORKDIR /build


RUN git clone https://git.ffmpeg.org/ffmpeg.git /build/ffmpeg && \
    cd /build/ffmpeg && \
    git checkout ${FFMPEG_GIT_REF}

ENV CC="ccache arm-linux-gnueabihf-gcc"
ENV CXX="ccache arm-linux-gnueabihf-g++"
ENV AR="arm-linux-gnueabihf-ar"
ENV RANLIB="arm-linux-gnueabihf-ranlib"
ENV STRIP="arm-linux-gnueabihf-strip"

RUN --mount=type=cache,target=/root/.ccache \
    cd /build/ffmpeg && \
    ./configure \
      --prefix=${FFMPEG_PREFIX} \
      --arch=arm \
      --target-os=linux \
      --cross-prefix=arm-linux-gnueabihf- \
      --enable-cross-compile \
      --pkg-config=pkg-config \
      --enable-shared \
      --disable-static \
      --disable-programs \
      --disable-doc \
      --disable-debug \
      --enable-pic \
      --disable-openssl --disable-gnutls  --disable-bzlib --disable-zlib\
      --sysroot=/usr/arm-linux-gnueabihf \
      --extra-cflags="-I/usr/include/arm-linux-gnueabihf" \
      --extra-ldflags="-L/usr/lib/arm-linux-gnueabihf"

RUN --mount=type=cache,target=/root/.ccache \
    cd /build/ffmpeg && make -j"$(nproc)" \
    && make install

RUN ls -l /opt/ffmpeg-armhf/lib/libavformat.so* && \
    ls -l /opt/ffmpeg-armhf/lib/pkgconfig/libavformat.pc && \
    PKG_CONFIG_LIBDIR=/opt/ffmpeg-armhf/lib/pkgconfig pkg-config --libs libavformat

#RUN exit 1
# back to nox

# -------------------------------------------------
# Fetch sources
# -------------------------------------------------
#RUN git clone https://github.com/neuromancer/nox-decomp.git
COPY . /build/nox-decomp

# -------------------------------------------------
# Build (armhf)
# -------------------------------------------------
WORKDIR /build/nox-decomp/build

RUN --mount=type=cache,target=/root/.ccache \
    cd /build/nox-decomp/build && \
    cmake .. \
      -DCMAKE_TOOLCHAIN_FILE=/toolchain-armhf.cmake \
      -DCMAKE_BUILD_TYPE=Debug \
      -DFFMPEG_PREFIX=/opt/ffmpeg-armhf \
      -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      -DCMAKE_C_FLAGS="\
        -U_FORTIFY_SOURCE \
        -fno-strict-aliasing \
        -fsigned-char \
        -O0 \
        -fno-tree-vectorize \
        -fno-aggressive-loop-optimizations \
      " \
      -DCMAKE_CXX_FLAGS="\
        -fno-strict-aliasing \
        -fsigned-char \
        -O0 \
      "

RUN --mount=type=cache,target=/root/.ccache \
    cd /build/nox-decomp/build && \
    cmake --build . -j $(nproc)


RUN mkdir -p /build/nox-decomp/build/src/lib && \
    cp -av ${FFMPEG_PREFIX}/lib/libav*.so* /build/nox-decomp/build/src/lib/ && \
    cp -av ${FFMPEG_PREFIX}/lib/libswscale.so* /build/nox-decomp/build/src/lib/ && \
    cp -av ${FFMPEG_PREFIX}/lib/libswresample.so* /build/nox-decomp/build/src/lib/
# -------------------------------------------------
# Verify output
# -------------------------------------------------
RUN find /build/nox-decomp/build/
RUN file /build/nox-decomp/build/src/out

#RUN arm-linux-gnueabihf-nm /build/nox-decomp/build/src/out | grep -E 'sub_4F4E50($|__)'
#RUN arm-linux-gnueabihf-nm /build/nox-decomp/build/src/out | grep 'sub_50A5C0'

RUN readelf -d /build/nox-decomp/build/src/out | grep NEEDED

RUN find  /opt/ffmpeg-armhf/lib/ && ls -lah /opt/ffmpeg-armhf/lib/




