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
      innoextract \
      zip && \
    rm -rf /var/lib/apt/lists/*


# Ensure pkg-config resolves ARMHF (prevents host/aarch64 contamination)
ENV PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig
ENV PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig
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

# -------------------------------------------------
# Verify output
# -------------------------------------------------
RUN find /build/nox-decomp/build/
RUN file /build/nox-decomp/build/src/out

#RUN arm-linux-gnueabihf-nm /build/nox-decomp/build/src/out | grep -E 'sub_4F4E50($|__)'
#RUN arm-linux-gnueabihf-nm /build/nox-decomp/build/src/out | grep 'sub_50A5C0'





