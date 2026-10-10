# Building & development

Developer notes. If you just want to play, grab a
[release zip](../../releases/latest) instead — see the install guide in
[README.md](README.md).

## Miyoo A30 build (armhf, Docker)

Cross-build the A30 binary from any machine:

```bash
docker run --rm --platform linux/arm/v7 \
  -v "$PWD":/work \
  ubuntu:16.04 bash /work/pc/build-a30-docker.sh
```

Output: `pc/build-a30/bin/AnimalCrossing`. Ubuntu 16.04 matches the A30's
glibc 2.23; the script fails if the binary needs anything newer. The first
run builds CMake and SDL2 headers from source into `pc/build-a30-tools/`
(slow under QEMU); later runs only rebuild the game.

Package it:

```bash
./a30/make-package.sh    # → a30/AnimalCrossing-ac-gc-a30.zip
```

Extract the zip to the SD card root (`Roms/A30PORTS/...`), or copy just the
binary over an existing install:

```bash
cp pc/build-a30/bin/AnimalCrossing "/Volumes/SDCARD/Roms/A30PORTS/ac-gc/AnimalCrossing" && sync
```

**Colima gotcha:** colima's docker only has the legacy builder, which
*silently ignores* `--platform` when an arm64 base image is already cached —
you end up with an arm64 build that looks fine until it hits the device.
Always verify the container arch:

```bash
docker run --rm --platform linux/arm/v7 ubuntu:16.04 uname -m   # must print: armv7l
```

## Test build (armhf, Debian, Docker)

The A30 binary links against the device's own SDL2 and GLES libraries, so it
can't run in a container. For headless testing, build the Debian armhf
binary — same code, same ABI:

```bash
docker run --rm --platform linux/arm/v7 \
  -v "$PWD":/work \
  debian:bookworm bash /work/pc/build-armhf-docker.sh
```

Output: `pc/build-armhf/bin/AnimalCrossing`. Run with `PC_GLES2=1` to force
the A30's GLES 2.0 renderer path (see Smoke tests).

## Desktop dev build (macOS)

```bash
./harness/run-native.sh
```

Needs Homebrew GCC and SDL2. Output: `pc/build/bin/AnimalCrossing`. Fastest
iteration loop for gameplay/logic work (64-bit, desktop GL).

## Smoke tests (harness/)

Headless Docker boot tests that launch the game under Xvfb and check it gets
past disc load:

```bash
./harness/smoke.sh armhf    # A30 ABI (slow, QEMU)
./harness/smoke.sh arm64    # near-native, GLES 3
```

After renderer changes, also run the armhf smoke with `PC_GLES2=1` (and
`MESA_GLES_VERSION_OVERRIDE=2.0` for a real GLES 2.0 context) — see
`kb/build-test.md` for the full matrix.

Put your GAFE01 USA disc image at `harness/rom/Animal Crossing.iso` — the
`harness/rom/` directory is git-ignored and its contents must **never** be
committed. Logs land in `harness/out/<arch>-smoke.log`. See
`harness/README.md` for the full setup (including one-time colima/binfmt
install).

## Cutting a release

Releases are built by CI (`.github/workflows/release.yml`):

1. Merge `dev` into `main`.
2. Tag on `main`: `git tag vX.Y.Z && git push origin vX.Y.Z`.
3. CI builds the A30 binary under QEMU (slow — up to a couple of hours),
   packages it via `a30/make-package.sh`, and publishes a GitHub release
   with `AnimalCrossing-ac-gc-a30-vX.Y.Z.zip` attached.

**Don't tag on `dev`** — the workflow triggers on any `v*` tag regardless of
branch, and releases should only come from `main`. Manual `workflow_dispatch`
runs upload the zip as a build artifact instead of creating a release.
