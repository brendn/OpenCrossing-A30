# Target device & deployment

## Community-confirmed devices (beyond the dev device)

- **RG28XX on Knulli** (2026-07-14 user report): "played for an hour,
  worked great" — confirms both a second H700 device (640×480 4:3, 2.83"
  screen) AND a second CFW (Knulli, not just muOS). Same binary, no
  changes. Listed in README supported-devices table.
- **RG35XX SP** (2026-07-14 user report): "works great", praised install
  instructions. No analog stick on this device — user needed the in-game
  Control menu toggle **"Use Dpad as Joystick"** to walk. Tip added to
  README. Done (2026-07-15): dpad_as_stick is now tri-state (0=off, 1=on,
  2=auto, default auto) — at controller open, auto enables
  dpad-as-joystick when SDL_JoystickNumAxes==0 or the mapping lacks
  leftx/lefty (SDL 2.0.0-safe APIs, no HasAxis). Explicit 0/1 in
  settings.ini is never overridden; old inis carrying 0 need Auto
  selected once in the Control menu (or an ini delete) to pick it up.
- **RG35XX (Plus/2024 line) on Knulli** (2026-07-15 user feedback via
  maintainer): "works perfectly stable". Exact model not specified —
  listed on the Plus/2024/Pro row in the README table. Knulli
  confirmations now: RG28XX, RG40XX V, RG35XX.
- **RG40XX H** (2026-07-19, issue #2): "works seamless on Gladiator 2 on
  the RG40XX-H" — first RG40XX H confirmation (previously "expected to
  work" from the shared H700 chip). OS reported as "Gladiator 2" — not a
  known CFW name (muOS/Knulli/ROCKNIX/stock are the usual ones); recorded
  verbatim, no log attached. README row upgraded to community-tested.
- **RG35XX H on modded stock OS** (2026-07-15, issue #1): tested up to
  house selection with a **.ciso** image (30MB) — confirms the .ciso path
  works on-device. Third H700 device and third OS (muOS, Knulli, modded
  stock). Log clean: SDL video driver `mali`, Mali-G31, GLES 3.2, 640×480
  fullscreen; one ALSA `snd_pcm_recover` underrun, nothing else. Quirk:
  **Select and Menu keys are reversed** on modded stock OS (applies to all
  PortMaster games there) — Menu/Function opens settings, Select+Start
  exits. Noted in README. Listed in README supported-devices table.

## Miyoo A30 (spruceOS) — experimental port, started 2026-10-07

**Device-verified (user report, 2026-10-08): runs at a mostly rock-solid
60 fps.** Also passes the Mesa GLES2 smoke matrix (kb/build-test.md). Some
notes below are still pre-device research; trust device logs over them.

- Hardware: Allwinner A33, 4x Cortex-A7 @ 1.2 GHz, **Mali-400 MP2 (GLES
  2.0 only, fp16-only fragment shaders)**, 512 MB RAM, 2.8" panel that is
  **480x640 portrait-scanned** (apps rotate their own output; there is no
  kernel rotation — mupen64plus uses `M64P_ROTATE=1`, GVU rotates by hand).
- OS: spruceOS, **glibc 2.23**, no PortMaster ("PortMaster app and games
  are not supported on the A30"). Ports are plain `.sh` files in
  `/mnt/SDCARD/Roms/A30PORTS/`, run with `/bin/sh` from that dir.
- Device SDL2 2.26.1 lives in `/usr/miyoo/lib`. The A30's working
  PPSSPP binary NEEDs `libGLESv2.so` + `libEGL.so` (unversioned) and
  nothing above GLIBC_2.17 — hence the stub-SONAME link in
  `pc/build-a30-docker.sh` and SDL_VIDEO_GL/EGL_DRIVER in the launcher.
- Input: buttons are keyboard keys. **Authoritative layout = spruce's
  `spruce/scripts/platform/A30.cfg`** (Linux key codes): A=Space(57)
  B=LCtrl(29) X=LShift(42) Y=LAlt(56) **L1=Tab(15) L2=E(18)
  R1=Backspace(14) R2=T(20)** Start=Return(28) Select=RCtrl(97)
  Menu=Escape(1), d-pad=arrows. NOT the Miyoo Mini layout — the first
  launcher assumed L1=E/R1=T/L2=Tab/R2=Backspace, so L1 (Tab) opened the
  settings menu and all four shoulders were mis-bound (2026-10-08 report).
  No stick click exists (spruce's B_L3/B_R3 are virtual keycodes).
- **Binding audit fixes (2026-10-08, device-confirmed 2026-10-10)**:
  new `Menu` keybinding (keybindings.ini, default Tab = old behaviour,
  rebindable in the Controls tab) replaces the hard-coded Tab menu toggle
  (pc_main.c; skipped while the Controls tab is capturing a key). A30
  launcher: L=Tab(L1) R=Backspace(R1) Z=T(R2) Menu=E(L2); an existing
  keybindings.ini without a `Menu` line is moved to keybindings.ini.old
  and regenerated once. `PC_NO_TYPING=1` (pc_typing.c) — on the A30, L1
  (Tab) in the in-game keyboard used to enter PC typing mode, turning
  A/L2/R2 into ' '/'e'/'t'. Escape (A30 Menu button) quits only on a
  second press within 2s when no SDL message box can be shown, with an
  overlay toast (SDL_QUIT/window close still quits immediately — that's
  how launchers stop us). Settings-menu tabs also switch with the keys
  bound to GC L/R; the menu key never switches tabs; holding the key
  bound to GC Z also clears a binding (A30 has no Delete / pad X). The stick is spruce's
  `joystickinput` daemon: `killall -USR2 joystickinput` = stick→d-pad,
  `-USR1` = analog. Launcher uses d-pad mode (game sees no SDL
  controller) and writes an A30 keybindings.ini on first run. Menu
  (Escape) quits the game without a save prompt.
- Launcher: `a30/Animal Crossing.sh` (the only launcher in this fork). Sets `PC_GLES2=1`, first-run
  settings: render_scale 75, fps dynamic, show_fps 1, verbose 1, LowSpec
  frustum_cull 1 / shadow_quality 1 / particle_quality 2.
- Rotation is auto (fullscreen + portrait drawable → 90° CCW, matching
  GVU). If the first device run is sideways/upside down: `PC_ROTATE=270`.
  If SDL reports 640x480 on the A30 the auto-detect won't fire — check
  the `[PC] Display rotation` / `Window created` lines in log.txt.
- **First device run (2026-10-07)**: SDL video driver `mali`, panel
  reported 480x640 → rotation 90 (FPS overlay correctly top-right), GLES
  2.0 context "Mali-400 MP / mali450-r5p1-01rel0", GLSL ES 1.00. Exts
  include OES_vertex_array_object, OES_texture_npot, OES_depth24,
  OES_packed_depth_stencil, OES_get_program_binary, OES_mapbuffer — no
  base_vertex. All 101 seed shaders compiled; FBO 480x360 complete;
  **60 FPS / 100% speed** on the title + intro. Sound + input worked.
  Screen was BLACK (overlay only): glUniformMatrix*fv(transpose=GL_TRUE)
  is GL_INVALID_VALUE on GLES2, so no matrix ever reached the shader —
  fixed by CPU-transposing (pc_gx.c upload_mat4/3_rowmajor). The same
  error had tripped the streaming-VBO probe; GLES2 now defaults to the
  legacy orphan-per-flush VBO path (PC_STREAM_VBO=1 opts in).
  /usr/lib/libGLESv2.so and libEGL.so → libMali.so (both versioned and
  unversioned names exist). SDL also exposes an "Atari Xbox 360 Game
  Controller" (spruce's joystickinput) with an analog stick — candidate
  for real analog walking instead of the d-pad-mode stick.
- **Double input (2026-10-07, after the black-screen fix: "it works!")**:
  that virtual pad ALSO reports the buttons, with a different layout, so
  each press hit two GC buttons — visible in the in-game keyboard (d-pad
  right also inserted a space = GC DRIGHT past end of text; A acted like
  B). Launcher now sets `PC_NO_CONTROLLER=1` (pc_pad.c: never open SDL
  game controllers → keyboard path only). If analog walking is wanted
  later, flip it: controller only + a keyboard-ignore switch, and verify
  the virtual pad's button layout from a log first.
- First things to read in an A30 log: which GLES attempt succeeded, the
  `[PC] GLES 2.0 path: vao=... program_binary=... npot=...` line, the GL
  extension list, `[GX] Render FBO: ... (INCOMPLETE!)`, and any
  `[PC/TEV] Specialized shader failed to compile` dumps.

## Hardware: Anbernic RG-34XX SP

- SoC: Allwinner H700, 4x Cortex-A53 (~1.5 GHz), armv7 userland (armhf)
- GPU: Mali-G31 MP2 — **GLES 3.2 only, no desktop GL**. Slow at dynamic
  branching in shaders (why TEV shaders are runtime-specialized, branchless).
  Slow shader compiler (why the program-binary disk cache exists).
- Screen: 3.4" 720x480. UI must be readable at 2x 8px font scale.
- OS: muOS with PortMaster runtime (armhf). PipeWire audio.

## SD card layout (volume name `ROMS`, FAT — mounts at /Volumes/ROMS on mac)

- `ports/ac-gc/` — `AnimalCrossing` binary, `libs.armhf/` (SDL2 2.26.5,
  libstdc++, libgcc fallbacks; device `/usr/lib32` takes precedence via
  LD_LIBRARY_PATH; GLES intentionally NOT bundled — device Mali driver),
  `rom/` (user's GAFE01 iso), `shaders/` (default.vert/frag), `conf/`.
  Game writes here: `log.txt`, `settings.ini`, `keybindings.ini`, `save/`,
  `shader_cache.bin`.
- `ROMS/Ports/Animal Crossing.sh` — PortMaster launcher (upstream's
  `port_files/` / `portmaster/`; removed from this fork 2026-10-10).

## Deploying a new build

```bash
cp pc/build-armhf/bin/AnimalCrossing "/Volumes/ROMS/ports/ac-gc/AnimalCrossing"
sync
```

Only the binary usually needs copying; shaders/launcher only when changed.
Do NOT copy a locally-generated `shader_cache.bin` to the device — binaries
are driver-specific (header driver-hash rejects mismatches, but it's noise).

## Audio (fixed 2026-07-13 — do not regress)

muOS PipeWire from a 32-bit process failed in `pw_loop_new` ("can't make
support.system handle") because the 32-bit SPA plugins weren't found. Fix
lives in the launcher: when the dirs exist it sets
`SPA_PLUGIN_DIR=/usr/lib32/spa-0.2`,
`PIPEWIRE_MODULE_DIR=/usr/lib32/pipewire-0.3`,
`SDL_AUDIODRIVER=pipewire,alsa,dsp`. ALSA alone does NOT work (muOS routes
the ALSA default through the same broken 32-bit pw plugin). The launcher also
dumps an "--- audio diag ---" block into log.txt.

## Reading device logs

`ports/ac-gc/log.txt` after a session. Useful lines:
- `[PC/TEV] Preloaded N shader(s) from disk cache` — shader cache working
- `[PC/TEV] Compiled specialized shader #N` — new TEV config (first-visit)
- `[STUTTER] frame N: total=..ms work=.. swap=.. gl=.. tex=.. draws=..` —
  only with verbose on; tex=ms high → texture decode burst, gl=ms high →
  driver/GL overhead, work high with others low → game logic
- `[PERF] ...` — periodic; needs verbose=1 (settings.ini or --verbose)
- `[Settings] Auto-detected display WxH (window_size=N)` — resolution
  auto-detect fired (stderr since 2026-07-15)
- `[PC] Controller '...': no analog stick detected, auto-enabling Dpad as
  Joystick` — dpad auto (only printed when setting is 2=auto)
- `[PC] BOOT ERROR: ...` (stderr) — disc image missing/unreadable/wrong
  version; the same text is shown full-screen on device (2026-07-19,
  DEVICE-VERIFIED same day: renamed iso → red error screen on RG-34XX SP,
  button exit worked, BOOT ERROR lines present in log.txt)
- `[PC] GCI: save copy N failed checksum` / `failed validation` — save
  rejected by load validation (2026-07-19); triage PC_NO_SAVE_VALIDATE=1
- `[PC] Special event scheduled: ...`, `[PC] EvMgr wade place: ...`,
  `[PC/TEX] ... EMPTY tlut slot N` — 2026-07-19 diagnostics (Redd-class
  reports, tourney placement, palette-item pink)

**verbose=0 discards stdout entirely (2026-07-19)**: pc_main freopens
stdout to /dev/null when verbose is off, so every OSReport/printf line —
GCI validation, `[PC] Reset detected!`, the 2026-07-19 diagnostics —
never reaches log.txt regardless of clean exit. Only stderr (boot lines,
BOOT ERROR, autodetect) survives. Any diagnostic capture session needs
`verbose = 1` in settings.ini first (flip back after — SD-backed stdout
costs a little CPU).

**Log truncation gotcha (2026-07-15)**: the stdout tail of log.txt (all
gameplay telemetry — PERF/STUTTER/PROF) is LOST on force-quit +
unclean unmount/power-off, even with line-buffered stdout. A device log
that ends mid-boot after a long session means lost tail, not a
boot-loop. Before pulling a log for analysis (especially the dock-hang
[PROF] capture): quit via the in-game exit, then eject the SD cleanly.
Boot-critical diagnostics print on stderr (unbuffered) for this reason.
