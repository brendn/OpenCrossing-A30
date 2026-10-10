#!/bin/bash
# Miyoo A30 build — run inside linux/arm/v7 ubuntu:16.04:
#
#   docker run --rm --platform linux/arm/v7 -v "$PWD":/work \
#     ubuntu:16.04 bash /work/pc/build-a30-docker.sh
#
# Output: pc/build-a30/bin/AnimalCrossing
#
# Why a separate build: the A30 (spruceOS) runs glibc 2.23 with no
# PortMaster runtime, so the H700 bookworm binary can't even load there.
# Ubuntu 16.04 IS glibc 2.23; the toolchain PPA supplies a C++17-capable
# GCC, and libstdc++/libgcc are linked statically (A30's are too old).
# CMake >= 3.16 and SDL2 headers are built from source once into
# pc/build-a30-tools/ (persisted on the host mount — first run is slow
# under qemu, later runs only rebuild the game).
set -x
export DEBIAN_FRONTEND=noninteractive
TOOLS=/work/pc/build-a30-tools
CMAKE_VER=3.16.9
SDL_VER=2.26.5
mkdir -p "$TOOLS"

apt-get update 2>&1 | tail -2
apt-get install -y --no-install-recommends \
  software-properties-common ca-certificates wget make pkg-config file binutils 2>&1 | tail -3
add-apt-repository -y ppa:ubuntu-toolchain-r/test 2>&1 | tail -2
apt-get update 2>&1 | tail -2
CC=; CXX=
for v in 9 8; do
  if apt-get install -y --no-install-recommends gcc-$v g++-$v 2>&1 | tail -2; then
    if command -v g++-$v >/dev/null; then CC=gcc-$v; CXX=g++-$v; break; fi
  fi
done
[ -n "$CC" ] || { echo "No C++17 GCC available from the toolchain PPA"; exit 1; }
export CC CXX

# --- CMake (xenial ships 3.5) ---
if [ ! -x "$TOOLS/cmake/bin/cmake" ]; then
  cd /tmp && wget -q "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VER/cmake-$CMAKE_VER.tar.gz" || exit 1
  tar xzf "cmake-$CMAKE_VER.tar.gz" && cd "cmake-$CMAKE_VER" || exit 1
  ./bootstrap --parallel=4 --prefix="$TOOLS/cmake" -- -DCMAKE_USE_OPENSSL=OFF 2>&1 | tail -3 || exit 1
  make -j4 2>&1 | tail -2 && make install 2>&1 | tail -1 || exit 1
fi
CMAKE="$TOOLS/cmake/bin/cmake"

# --- SDL2: only headers + an import library are needed; the device's own
# SDL2 (/usr/miyoo/lib, A30-specific video backend) is used at runtime ---
if [ ! -f "$TOOLS/sdl2/lib/libSDL2.so" ]; then
  cd /tmp && wget -q "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz" || exit 1
  tar xzf "SDL2-$SDL_VER.tar.gz" && cd "SDL2-$SDL_VER" || exit 1
  ./configure --prefix="$TOOLS/sdl2" --disable-static \
    --disable-video-x11 --disable-video-wayland --disable-video-opengl \
    --disable-pulseaudio --disable-jack --disable-sndio --disable-esd 2>&1 | tail -3 || exit 1
  make -j4 2>&1 | tail -2 && make install 2>&1 | tail -1 || exit 1
fi

# --- GLES: the A30's driver ships libGLESv2.so / libEGL.so (unversioned —
# the device's working PPSSPP build NEEDs exactly those names). glad loads
# every GL entry point through SDL_GL_GetProcAddress, so link an empty stub
# with that SONAME purely to get the right NEEDED entry. ---
mkdir -p "$TOOLS/stubs"
echo "" | $CC -x c -shared -Wl,-soname,libGLESv2.so -o "$TOOLS/stubs/libGLESv2.so" -

mkdir -p /work/pc/build-a30
cd /work/pc/build-a30 || exit 1
# Cortex-A7 with NEON (vfpv4); same -fsigned-char rationale as the H700 build.
TUNE="-fsigned-char -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard"
"$CMAKE" .. -DPC_USE_GLES=ON \
  -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_PREFIX_PATH="$TOOLS/sdl2" \
  -DSDL2_DIR="$TOOLS/sdl2/lib/cmake/SDL2" \
  -DGLESV2_LIB="$TOOLS/stubs/libGLESv2.so" \
  -DCMAKE_C_FLAGS="$TUNE" \
  -DCMAKE_CXX_FLAGS="$TUNE" \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" 2>&1 | tail -20 || exit 1
make -j4 2>&1 | tee /work/pc/build-a30/make.log | grep -E "^\[|Error|error:|Built target" | tail -40
MAKE_RC=${PIPESTATUS[0]}
echo "=== MAKE EXIT: $MAKE_RC ==="
if [ "$MAKE_RC" -ne 0 ]; then
  echo "=== ERROR CONTEXT ==="
  grep -B3 -A5 -E "error:|Error " /work/pc/build-a30/make.log | tail -80
  exit $MAKE_RC
fi

BIN=/work/pc/build-a30/bin/AnimalCrossing
echo "=== FILE OUTPUT ==="
file "$BIN"
echo "=== READELF NEEDED ==="
readelf -d "$BIN" | grep NEEDED
echo "=== GLIBC VERSIONS REQUIRED (A30 has 2.23) ==="
MAXV=$(objdump -T "$BIN" | grep -oE "GLIBC_2\.[0-9]+" | sort -t. -k2 -n -u | tail -1)
echo "highest: $MAXV"
if [ "$(echo "$MAXV" | cut -d. -f2)" -gt 23 ]; then
  echo "ERROR: binary needs $MAXV — will not load on the A30"; exit 1
fi
if objdump -T "$BIN" | grep -q "GLIBCXX\|CXXABI"; then
  echo "ERROR: dynamic libstdc++ symbols present (expected static)"; exit 1
fi
echo "=== A30 BUILD OK ==="
