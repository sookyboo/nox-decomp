# Cross-compilation sandbox setup

This guide lists the host packages and target libraries needed to build Nox
Decomp in an Ubuntu/Debian sandbox for:

- Linux i386 (`i686-linux-gnu`)
- Linux ARM hard-float (`arm-linux-gnueabihf`, normally ARMv7/armhf)
- Windows i386 (`i686-w64-mingw32`)

Windows ARM is not in scope. Use a separate CMake build directory and
`pkg-config` search path for each target.

## Setup performed in the development sandbox

The working environment is Ubuntu 26.04 (Resolute) on amd64. The following
additional setup was required to build and test the architecture paths:

```sh
sudo dpkg --add-architecture i386
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential cmake ninja-build pkg-config git \
  gcc-i686-linux-gnu g++-i686-linux-gnu binutils-i686-linux-gnu \
  libc6-dev-i386-cross linux-libc-dev-i386-cross \
  libsdl2-dev:i386 libopenal-dev:i386 libgl-dev:i386 \
  libavformat-dev:i386 libavcodec-dev:i386 libavutil-dev:i386 \
  libswscale-dev:i386 libswresample-dev:i386 zlib1g-dev:i386
```

For ARMHF ABI cross-tests, install the compiler, ARM sysroot, and emulator:

```sh
sudo apt-get install -y --no-install-recommends \
  gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf \
  binutils-arm-linux-gnueabihf libc6-dev-armhf-cross \
  qemu-user
```

If the Ports archive does not provide `libsdl2-dev:armhf` for the sandbox's
codename, SDL2 2.26.2 can be built from source using the same cross-build shape
as `Dockerfile.x86libs`:

```sh
CFLAGS='-std=gnu17' ./configure \
  --build="$(dpkg-architecture -qDEB_BUILD_GNU_TYPE)" \
  --host=arm-linux-gnueabihf \
  --prefix=/opt/sdl2-armhf --libdir=/opt/sdl2-armhf/lib \
  --enable-shared --disable-static \
  --disable-libsamplerate --disable-alsa \
  --disable-video-x11 --disable-video-wayland \
  --disable-video-opengl --disable-video-vulkan \
  --disable-video-rpi --disable-video-kmsdrm
make -j"$(nproc)"
sudo make install
```

`CFLAGS=-std=gnu17` is needed with the Ubuntu 26.04 compiler because SDL2
2.26.2 contains an older `false` enum declaration that is rejected under the
default C23 mode. The `--disable-alsa` and `--disable-libsamplerate` options
avoid unavailable ARMHF development libraries; the rendering regression test
only needs SDL surfaces and does not require those backends.

The resulting ARMHF SDL2 test binary can be run without an ARM machine:

```sh
arm-linux-gnueabihf-gcc -mfloat-abi=hard -O2 \
  tests/render_arch_test.c -I/opt/sdl2-armhf/include \
  -L/opt/sdl2-armhf/lib -Wl,-rpath,/opt/sdl2-armhf/lib -lSDL2 \
  -o /tmp/render-arch-armhf
qemu-arm -L /usr/arm-linux-gnueabihf /tmp/render-arch-armhf
```

Keep architecture-specific builds separate (`build-i386`, `build-armhf`) and
restrict `PKG_CONFIG_LIBDIR` to the target library directories to prevent host
amd64 libraries from being selected.

## Enable Linux target architectures

```sh
sudo dpkg --add-architecture i386
sudo dpkg --add-architecture armhf
sudo apt-get update
```

On Ubuntu, `archive.ubuntu.com` carries amd64/i386 while ARMHF is served by
`ports.ubuntu.com`. Scope the normal deb822 source stanzas with
`Architectures: amd64 i386` and add an ARMHF stanza similar to:

```text
Types: deb
Architectures: armhf
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: resolute resolute-updates resolute-backports resolute-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

Replace `resolute` with the sandbox's Ubuntu codename when necessary.

## Common build tools

```sh
sudo apt-get install -y --no-install-recommends \
  build-essential autoconf automake libtool binutils \
  ca-certificates ccache cmake file gettext git innoextract \
  make nasm yasm ninja-build pkg-config python3 texinfo \
  unzip zip wget
```

CMake and pkg-config are direct build requirements. Autotools, NASM, YASM,
and Texinfo are needed when FFmpeg or another dependency is built from source.

## Linux i386 packages

```sh
sudo apt-get install -y --no-install-recommends \
  gcc-i686-linux-gnu \
  g++-i686-linux-gnu \
  binutils-i686-linux-gnu \
  libc6-dev-i386-cross \
  linux-libc-dev-i386-cross \
  libsdl2-dev:i386 \
  libopenal-dev:i386 \
  libgl-dev:i386 \
  libavformat-dev:i386 \
  libavcodec-dev:i386 \
  libavutil-dev:i386 \
  libswscale-dev:i386 \
  libswresample-dev:i386 \
  zlib1g-dev:i386
```

Ubuntu 26.04 does not publish `libglew-dev:i386`. Build GLEW for i686 into a
target prefix when using desktop OpenGL, or use the project's gl4es route.

Configure using only i386 package metadata:

```sh
export PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig:/usr/share/pkgconfig
unset PKG_CONFIG_PATH

cmake -S . -B build-i386 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=i686-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=i686-linux-gnu-g++
cmake --build build-i386 -j"$(nproc)"
```

The project adds `-m32` for x86 targets. Explicit cross compilers help ensure
that headers and libraries come from i386 rather than the amd64 host.

## Linux ARM32 hard-float packages

```sh
sudo apt-get install -y --no-install-recommends \
  gcc-arm-linux-gnueabihf \
  g++-arm-linux-gnueabihf \
  binutils-arm-linux-gnueabihf \
  libc6-dev-armhf-cross \
  linux-libc-dev-armhf-cross \
  libsdl2-dev:armhf \
  libopenal-dev:armhf \
  libegl1-mesa-dev:armhf \
  libgles2-mesa-dev:armhf \
  libgbm-dev:armhf \
  libdrm-dev:armhf \
  libx11-dev:armhf \
  libxext-dev:armhf \
  libxfixes-dev:armhf \
  libxdamage-dev:armhf \
  libxxf86vm-dev:armhf \
  libavformat-dev:armhf \
  libavcodec-dev:armhf \
  libavutil-dev:armhf \
  libswscale-dev:armhf \
  libswresample-dev:armhf \
  zlib1g-dev:armhf
```

The Mesa and X11 packages mirror the root Dockerfile's gl4es build. They may
be omitted when gl4es will not be built or packaged.

### Install the ARMHF link libraries in this sandbox

This sandbox's Ubuntu release publishes ARMHF packages from
`ports.ubuntu.com`, not from `archive.ubuntu.com`. Keep the normal source
stanzas limited to `amd64 i386`, then add an ARMHF deb822 source:

```text
Types: deb
Architectures: armhf
URIs: http://ports.ubuntu.com/ubuntu-ports/
Suites: resolute resolute-updates resolute-backports resolute-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

Save it as `/etc/apt/sources.list.d/armhf.sources`, add the architecture, and
refresh the package index:

```sh
sudo dpkg --add-architecture armhf
sudo apt-get update
```

Install the ARMHF development packages used by the normal CMake link. This
installs OpenAL, GL/Mesa/GLVND, FFmpeg, and zlib into the ARMHF sysroot under
`/usr/lib/arm-linux-gnueabihf`:

```sh
sudo apt-get install -y --no-install-recommends \
  libsdl2-dev:armhf \
  libopenal-dev:armhf \
  libgl1-mesa-dev:armhf \
  libavformat-dev:armhf \
  libavcodec-dev:armhf \
  libavutil-dev:armhf \
  libswscale-dev:armhf \
  libswresample-dev:armhf \
  zlib1g-dev:armhf
```

Verify that the linker names resolve to ARM ELF libraries:

```sh
file \
  /usr/lib/arm-linux-gnueabihf/libopenal.so* \
  /usr/lib/arm-linux-gnueabihf/libGL.so* \
  /usr/lib/arm-linux-gnueabihf/libavformat.so* \
  /usr/lib/arm-linux-gnueabihf/libavcodec.so* \
  /usr/lib/arm-linux-gnueabihf/libavutil.so* \
  /usr/lib/arm-linux-gnueabihf/libswscale.so* \
  /usr/lib/arm-linux-gnueabihf/libswresample.so* \
  /usr/lib/arm-linux-gnueabihf/libz.so*

PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig \
  pkg-config --modversion openal gl libavformat libavcodec libavutil libswscale libswresample zlib
```

The current Resolute Ports archive provides `libsdl2-dev:armhf` 2.32.10, so
the source build above is not needed in this sandbox. A package-only local
ARMHF build can use the system target metadata directly:

```sh
export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
unset PKG_CONFIG_PATH

cmake -S . -B build-armhf -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_SYSTEM_PROCESSOR=arm \
  -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
  -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
  -DCMAKE_CROSSCOMPILING_EMULATOR='qemu-arm;-L;/usr/arm-linux-gnueabihf'
cmake --build build-armhf -j"$(nproc)"
ctest --test-dir build-armhf --output-on-failure
```

`CMAKE_CROSSCOMPILING_EMULATOR` lets CTest run the ARMHF executables through
QEMU. If it is omitted, invoke each test with
`qemu-arm -L /usr/arm-linux-gnueabihf` instead of running `ctest` directly.

The complete CTest suite currently contains 26 tests. In the development
sandbox, the i386 build passes all 26 tests. The ARMHF build also compiles and
executes all 26 tests under QEMU; 24 pass, while `summon_update_test` exits
with a segmentation fault and `summon_behavior_test` exits with a bus error.
These failures are ARMHF runtime issues, not build or test-discovery failures,
and should be investigated before treating the ARMHF suite as passing.

The root `Dockerfile` also contains an optional gl4es build for packaging a
software-compatible `libGL.so.1`. That is a runtime/package choice; the
system Mesa/GLVND development package above is sufficient to link the ARMHF
game and tests in this sandbox. OpenAL Soft can likewise be built from source
using the commented Dockerfile recipe, but the packaged `libopenal-dev:armhf`
path is the reproducible local setup used here.

### Build FFmpeg for ARMHF

For release parity, build FFmpeg 7.1.1 from source instead of using the
distribution ARM packages. The container recipe uses the same configuration:
shared libraries only, no command-line programs or documentation, and no
OpenSSL, GnuTLS, BZip2, or zlib dependencies.

```sh
export FFMPEG_PREFIX=/opt/ffmpeg-armhf
export FFMPEG_SRC=/tmp/ffmpeg-armhf

git clone --branch n7.1.1 https://git.ffmpeg.org/ffmpeg.git "$FFMPEG_SRC"
cd "$FFMPEG_SRC"

PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig \
CC="ccache arm-linux-gnueabihf-gcc" \
CXX="ccache arm-linux-gnueabihf-g++" \
AR=arm-linux-gnueabihf-ar \
RANLIB=arm-linux-gnueabihf-ranlib \
STRIP=arm-linux-gnueabihf-strip \
./configure \
  --prefix="$FFMPEG_PREFIX" \
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
  --disable-openssl \
  --disable-gnutls \
  --disable-bzlib \
  --disable-zlib \
  --extra-cflags="-I/usr/arm-linux-gnueabihf/include" \
  --extra-ldflags="-L/usr/lib/arm-linux-gnueabihf"

make -j"$(nproc)"
sudo make install
```

On this Debian/Ubuntu cross-toolchain layout, do not pass
`--sysroot=/usr/arm-linux-gnueabihf`: headers are under
`/usr/arm-linux-gnueabihf`, while target libraries are under
`/usr/lib/arm-linux-gnueabihf`. Verify the installation before configuring the
project:

```sh
PKG_CONFIG_LIBDIR="$FFMPEG_PREFIX/lib/pkgconfig" \
  pkg-config --modversion libavformat libavcodec libavutil libswscale libswresample
file "$FFMPEG_PREFIX/lib/libavformat.so"
```

Create `build-toolchains/armhf.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_FIND_ROOT_PATH
    /usr/arm-linux-gnueabihf
    /usr/lib/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Then configure the target:

```sh
export PKG_CONFIG_LIBDIR=/opt/ffmpeg-armhf/lib/pkgconfig:/opt/sdl2-armhf/lib/pkgconfig:/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
export PKG_CONFIG_PATH=/opt/ffmpeg-armhf/lib/pkgconfig:/opt/sdl2-armhf/lib/pkgconfig

cmake -S . -B build-armhf -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build-toolchains/armhf.cmake" \
  -DFFMPEG_PREFIX=/opt/ffmpeg-armhf
cmake --build build-armhf -j"$(nproc)"
```

The release Docker build packages gl4es as the ARM OpenGL implementation.
Building the executable and packaging the resulting `libGL.so.1` are separate
operations.

## Windows i386 packages

```sh
sudo apt-get install -y --no-install-recommends \
  gcc-mingw-w64-i686 \
  g++-mingw-w64-i686 \
  binutils-mingw-w64-i686 \
  mingw-w64-tools
```

Ubuntu supplies the compiler and Windows import libraries, but not every
third-party Windows library needed by this project. Build or unpack these into
the prefixes used by `Dockerfile.winx86`:

| Library | Prefix | Required contents |
| --- | --- | --- |
| FFmpeg 7.1.1 | `/opt/ffmpeg-win32` | Headers, `.pc` files, import libraries, DLLs |
| SDL2 2.30.11 | `/opt/sdl2-win32` | Headers, import library, `sdl2.pc`, `SDL2.dll` |
| OpenAL Soft 1.22.2 | `/opt/openal-win32` | Headers, import library, `OpenAL32.dll` |
| GLEW 2.1.0 | `/opt/glew-win32` | `GL/glew.h`, import library, DLL |

Those versions match the current Dockerfile. Once the prefixes exist:

```sh
export PKG_CONFIG_LIBDIR=/opt/ffmpeg-win32/lib/pkgconfig:/opt/sdl2-win32/lib/pkgconfig
unset PKG_CONFIG_PATH

cmake -S . -B build-win32 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=i686-w64-mingw32-windres \
  -DCMAKE_PREFIX_PATH="/opt/sdl2-win32;/opt/openal-win32;/opt/glew-win32;/opt/ffmpeg-win32" \
  -DFFMPEG_PREFIX=/opt/ffmpeg-win32 \
  -DCMAKE_C_FLAGS="-I/opt/sdl2-win32/include -I/opt/openal-win32/include -I/opt/glew-win32/include" \
  -DCMAKE_CXX_FLAGS="-I/opt/sdl2-win32/include -I/opt/openal-win32/include -I/opt/glew-win32/include" \
  -DCMAKE_EXE_LINKER_FLAGS="-L/opt/sdl2-win32/lib -L/opt/openal-win32/lib -L/opt/glew-win32/lib -L/opt/ffmpeg-win32/lib" \
  -DUSE_SDL=ON \
  -DUSE_DIRECTX=OFF
cmake --build build-win32 -j"$(nproc)"
```

To assemble the runnable i386 bundle used by the Windows launcher, copy the
built executable under the expected name and include the runtime DLLs and
assets from the Dockerfile's staging directory:

```sh
mkdir -p build-win32/src/noxd.win32
cp build-win32/src/out.exe build-win32/src/noxd.win32/noxd.i386.exe
cp dist-scripts/nox.cfg build-win32/src/noxd.win32/nox.cfg
```

The launcher may maintain a generated config under `gamefiles/app`; refresh it
when testing input changes:

```sh
cp build-win32/src/noxd.win32/nox.cfg \
  build-win32/src/noxd.win32/gamefiles/app/nox.cfg
```

Verify the executable architecture and imported runtime DLLs:

```sh
file build-win32/src/noxd.win32/noxd.i386.exe
i686-w64-mingw32-objdump -p build-win32/src/noxd.win32/noxd.i386.exe \
  | grep 'DLL Name'
```

Package the FFmpeg, SDL2, OpenAL, GLEW, and MinGW runtime DLLs with the
resulting executable. The final section of `Dockerfile.winx86` contains the
current copy logic.

Check imports rather than assuming a GCC exception model:

```sh
i686-w64-mingw32-objdump -p build-win32/src/out.exe | grep 'DLL Name'
```

Ubuntu 26.04's default i686 MinGW compiler uses `libgcc_s_dw2-1.dll`. Older
toolchains may use `libgcc_s_sjlj-1.dll`; package whichever appears in the
import table.

## FFmpeg choice

The Linux package lists above use distribution FFmpeg libraries for quicker
local builds. Release Docker images instead build FFmpeg 7.1.1 into:

- `/opt/ffmpeg-i386`
- `/opt/ffmpeg-armhf`
- `/opt/ffmpeg-win32`

Use those source builds for release parity or when the distribution build does
not provide the required VQA behavior. If using a source-built FFmpeg, omit the
corresponding `libav*` and `libsw*` development packages and pass its prefix as
`-DFFMPEG_PREFIX=...`.

Never place multiple targets' directories in the same `PKG_CONFIG_LIBDIR`.

## Verify the toolchains and outputs

```sh
i686-linux-gnu-gcc --version
arm-linux-gnueabihf-gcc --version
i686-w64-mingw32-gcc --version
cmake --version
ninja --version
pkg-config --version

file build-i386/src/out
file build-armhf/src/out
file build-win32/src/out.exe
```
