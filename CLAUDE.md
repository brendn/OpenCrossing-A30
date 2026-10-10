# CLAUDE.md

**OpenCrossing-A30** (repo brendn/OpenCrossing-A30, forked 2026-10-10 from
TechProGabe/OpenCrossing-Anbernic, formerly GabeConway/OpenCrossing-Anbernic)
— native Animal Crossing (GameCube decomp) build for the **Miyoo A30**
(Allwinner A33 Cortex-A7 armhf, Mali-400 MP2 GLES 2.0 only, 480x640
portrait-scanned panel, spruceOS, glibc 2.23, no PortMaster). Upstream is an
Anbernic H700 port (RG-34XX SP, Mali-G31 GLES 3.x, muOS + PortMaster),
itself bootstrapped from the Dia2809 fork of flyngmt/ACGC-PC-Port (all
credited in README). The A30 support is a GLES 2.0 path in the same
codebase (`PC_GLES2=1`, `pc_gl_compat.c`); the GLES3/H700 path is kept
working and byte-identical so changes can flow back upstream. Runtime paths
(`ports/ac-gc/`, `Roms/A30PORTS/ac-gc/`, launcher name "Animal Crossing.sh")
are deliberately stable — renaming them would break existing installs.

## Ground rules

- Branches: `main` = releases (CI builds on `v*` tags only — conserve
  Actions minutes), `dev` = daily work. Never tag dev.
- **Never commit ROM material** (`harness/rom/`, SD card contents).
- Decomp game code (`src/`) is vendored: gnu89, UB-dependent (keep the CMake
  UB-guard flags), 32-bit pointer assumptions everywhere. Platform code lives
  in `pc/`.
- After any renderer/texture/shader change: build armhf in Docker and run
  `./harness/smoke.sh armhf` (twice, to also exercise the shader disk cache),
  plus the GLES2 runs in kb/build-test.md (`PC_GLES2=1`). The A30 binary
  can't run in the Mesa container — the armhf build is the GLES2 test bed.
- Anbernic packaging (`portmaster/`, `port_files/`, `release.sh`) was removed
  from this fork; the A30 launcher is `a30/Animal Crossing.sh`. Shared `pc/`
  code still supports the H700/GLES3 path — don't strip it (upstream merges).
- A30 packaging: `a30/make-package.sh` builds the zip in a temp dir; never
  commit `a30/Roms/` or `a30/*.zip` (gitignored).

## Knowledge base — read on demand, don't guess

Load the relevant file before working in its area:

| File | Read when |
|------|-----------|
| `kb/device.md` | Deploying to SD, audio/PipeWire, device hardware limits, reading log.txt |
| `kb/renderer.md` | Touching pc_gx*, TEV shaders, shader/texture caches, batching |
| `kb/perf.md` | Any fps/stutter work — what's fixed, how to measure, open ideas, gotchas |
| `kb/build-test.md` | Building (flags! colima gotcha), harness, CI/release/packaging |
| `kb/game.md` | Touching decomp code, emu64, saves, settings enums, texture packs |
| `kb/history.md` | Why things are the way they are; decisions log |
| `kb/issues.md` | Open bugs with leads, resolved-issue patterns |

Keep kb/ files updated when the facts change — they are the project memory.

## Quick reference

```bash
# device build (output: pc/build-armhf/bin/AnimalCrossing)
docker run --rm --platform linux/arm/v7 -v "$PWD":/work \
  debian:bookworm bash /work/pc/build-armhf-docker.sh
# boot test with rom
./harness/smoke.sh armhf
# Miyoo A30 build (glibc 2.23, GLES2 path) — see kb/device.md "Miyoo A30"
docker run --rm --platform linux/arm/v7 -v "$PWD":/work \
  ubuntu:16.04 bash /work/pc/build-a30-docker.sh
# A30 release zip (output: a30/AnimalCrossing-ac-gc-a30.zip)
./a30/make-package.sh
# deploy to A30 (spruceOS SD card mounted as SDCARD)
cp pc/build-a30/bin/AnimalCrossing "/Volumes/SDCARD/Roms/A30PORTS/ac-gc/AnimalCrossing" && sync
```

## Status (2026-10-10)

Works on the A30: rendering, controls (binding fixes device-confirmed),
saves, audio — ~60 fps at 100% render scale; launcher defaults to 75% for
battery. Upstream H700 status/perf history: kb/perf.md, kb/history.md.
