# Building & testing

## Device build (armhf, the one that matters)

```bash
docker run --rm --platform linux/arm/v7 \
  -v "$PWD":/work \
  debian:bookworm bash /work/pc/build-armhf-docker.sh
```

Output `pc/build-armhf/bin/AnimalCrossing` (ELF 32-bit ARM EABI5 hard-float).
Build dir persists on the host mount → incremental. The script also sets the
target tuning flags (`-mcpu=cortex-a53 -mfpu=neon-vfpv4 -mfloat-abi=hard`)
and runs an in-container smoke attempt (its xvfb run may fail on missing
xauth — that's the container, not the binary; use the harness instead).

**Colima/docker gotcha**: `exec format error` from an arm container means
qemu binfmt handlers aren't registered in the colima VM — run
`docker run --privileged --rm tonistiigi/binfmt --install arm` once per VM
(2026-07-15). **Needed again after every `colima stop`/`start`** — the
registration doesn't survive a VM restart (2026-10-08: both builds failed
with exec format error right after `colima start`). Also: legacy builder (no buildx) silently ignores
`--platform` when an arm64 base image is cached. Verify any image/container
with `uname -m` → must print `armv7l`. `acgc-smoke-deps:armhf` was once
silently arm64 and had to be rebuilt via `docker commit` from an explicit
`--platform linux/arm/v7` container.

## Miyoo A30 build (2026-10-07)

```bash
docker run --rm --platform linux/arm/v7 -v "$PWD":/work \
  ubuntu:16.04 bash /work/pc/build-a30-docker.sh
./a30/make-package.sh   # → a30/AnimalCrossing-ac-gc-a30.zip
```

Ubuntu 16.04 = glibc 2.23 (the A30's). GCC 9 from the
ubuntu-toolchain-r PPA (C++17), static libstdc++/libgcc, Cortex-A7 tuning.
CMake 3.16 and SDL2 2.26.5 (import lib + headers only) are built from
source into `pc/build-a30-tools/` on first run (slow under qemu, cached
after). Links a stub `libGLESv2.so` SONAME (see kb/device.md). The script
fails if the binary needs > GLIBC_2.23 or dynamic libstdc++.
First run 2026-10-07: OK (GCC 9.4.0; highest symbol GLIBC_2.17 — same
ceiling as the A30's PPSSPP; NEEDED libSDL2-2.0.so.0, libGLESv2.so, libdl,
libpthread, libm, libc). First run ~1h under qemu (CMake+SDL2 from source,
then cached in pc/build-a30-tools/); apt + PPA setup repeats every run.
The macOS native build can't work on Apple Silicon (32-bit only project).

GLES2 path in the smoke harness: run the armhf smoke with `PC_GLES2=1`
(and once with `PC_GLES2_NO_VAO=1`) in the container env — Mesa hands
back a 3.x context, the flag forces the GLES2 code path anyway.
Rotation can be exercised the same way with `PC_ROTATE=90`.
**Also run with `-e MESA_GLES_VERSION_OVERRIDE=2.0`**: that gives a real
GLES 2.0 context with GLES2 validation. PC_GLES2=1 alone on a 3.x context
missed the transpose=GL_TRUE matrix uploads that black-screened the A30.
2026-10-07 matrix (armhf, Mesa 22.3.6): GLES3 x2, PC_GLES2=1 x2, and
PC_GLES2=1+PC_GLES2_NO_VAO=1+PC_ROTATE=90 all survived 120s into the
title screen with 0 shader compile/link failures on the GLES2 path;
disk cache preloads on 2nd run in both modes. Xvfb screenshots (xwd via
x11-apps in the container) showed GLES2 matching GLES3 and correct 90°
CCW rotation. Known Mesa-only noise: the GLES3 uber-shader fallback fails
to link ("u_light_pos mismatching precision") — pre-existing, Mali
accepts it, specialized shaders unaffected.

## Test harness (`harness/`)

- `./harness/smoke.sh armhf` — boots the real binary + rom headless in
  Docker (image `acgc-smoke-deps:armhf`), 120s; PASS = survived past disc
  load, log at `harness/out/armhf-smoke.log`. **Run this after every build.**
- `./harness/smoke.sh arm64` — same for the arm64 GLES build.
- `./harness/run-native.sh` — macOS native build, fastest iteration loop.
- Rom: `harness/rom/Animal Crossing.iso` (GAFE01 USA, 1459978240 bytes).
  Git-ignored. **Never commit anything under harness/rom/.**
- `./harness/check-launcher-sync.sh` — normalized diff of the three launcher
  .sh copies (GAMEDIR/PORT_32BIT lines intentionally differ). Run before
  every release package.
- `./harness/inspect-gci.py <gci>` — save forensics (equipment/pockets dump).
- Verifying the shader disk cache end-to-end: run smoke twice; second run's
  log must show `[PC/TEV] Preloaded N shader(s) from disk cache`
  (cache file lands in `pc/build-armhf/bin/shader_cache.bin`).
- Verifying resolution auto-detect in smoke (2026-07-15): comment out
  `window_width/height/size` in `pc/build-armhf/bin/settings.ini` and set
  `fullscreen = 1` → log must show `[Settings] Auto-detected display WxH`
  (Xvfb screen is 1280x1024 → window_size=6 custom). Restore the ini after.
  Smoke can NOT exercise gameplay paths (no scripted input) — shovel-dupe
  stow, clock long-run, and dpad-on-stickless remain device/human tests.

## Desktop/macOS build

`./build_pc.sh` (needs SDL2). Apple linker guards live in pc/CMakeLists.txt
(`--version-script`/`--allow-multiple-definition` are GNU-ld-only) and
pc/src/pc_stubs.c (`#ifndef __APPLE__` cKF stubs).

## Compile flags (don't regress)

- `CMAKE_C(XX)_FLAGS_RELEASE` in pc/CMakeLists.txt MUST keep an explicit
  `-O1` — these overrides replace CMake's defaults entirely, and they
  historically shipped with no -O at all (game code at -O0).
- The UB-guard flags next to it (-fno-delete-null-pointer-checks etc.) exist
  because decomp code relies on UB; keep them when touching optimization.
- `pc_gx_texture.c` gets per-source -O3 (set_source_files_properties uses the
  `${CMAKE_CURRENT_SOURCE_DIR}/src/...` absolute form — must match how
  PC_SOURCES lists it).
- Game (decomp) code is capped at **-O1** — -O2 crash-loops on device
  (see kb/perf.md "-O2 regression").

## Shader seed (pc/shaders/shader_seed.bin)

Keys-only copy of a shader_cache.bin (same header, per-entry blob length 0).
Compiled during the boot splash on first launch so gameplay never compiles a
known shader. Regenerate after content that introduces new TEV configs:
take a device shader_cache.bin that has seen the new content and strip the
blobs (each entry = 72-byte key + two u32 meta + blob; write meta {0,0} and
no blob; dedupe keys; keep the 12-byte header). Bump SDC_VERSION in
pc_gx_tev.c whenever ShaderKey layout or generated GLSL changes — that
invalidates both local caches and the seed format together.

## CI / releases

`.github/workflows/release.yml`: triggers ONLY on `v*` tags (+ manual
dispatch → artifact instead of release) to conserve private-repo Actions
minutes. QEMU + same docker script, ~1-3h. Packages via
`portmaster/make-package.sh` → zip with `Animal Crossing.sh` at root +
`ports/ac-gc/{AnimalCrossing,shaders/,rom/README.txt,libs.armhf/README.txt}`.
Release flow: merge dev→main, tag `vX.Y.Z` on main, push tag. Never tag dev.
Launcher sync rule: `port_files/Animal Crossing.sh` is source of truth;
`portmaster/Animal Crossing.sh` ships in the zip — keep in sync (a stale copy
once nearly shipped without the PipeWire audio fix).
