# Renderer architecture (GX → GLES translation layer)

All in `pc/src/` + `pc/include/pc_gx_internal.h`. The game's decompiled code
calls the GameCube GX API; these files implement it on GL/GLES.

## Pipeline overview

- `pc_gx.c` — GX state machine (`g_gx` global `PCGXState`), vertex
  submission (GXBegin/GXPosition*/GXEnd), draw dispatch
  (`pc_gx_flush_vertices`), dirty-flag uniform upload, framebuffer copies.
- `pc_gx_tev.c` — TEV combiner → fragment shader. Generates a small
  **branchless GLSL shader per unique TEV config** (~30-50 configs in AC)
  instead of an uber-shader; Mali hates dynamic branching. 256-slot in-memory
  cache keyed by 72-byte `ShaderKey`.
- `pc_gx_texture.c` — GC texture format decoders (I4/I8/IA4/IA8/RGB565/
  RGB5A3/RGBA8/CI4/CI8/CMPR, tiled → linear RGBA8) + 2048-entry texture cache.
- `pc_gx_mtx.c`, `pc_vi.c` (frame pacing/swap/dynamic fps), `pc_overlay.c`
  (bitmap-font FPS counter + settings menu + boot splash).

## Batching & draw dispatch

- Vertices accumulate in `g_gx.vertex_buffer` (64k cap, 48-byte PCGXVertex).
- emu64 usually omits GXEnd; `pc_gx_flush_if_begin_complete()` flushes when
  the declared vertex count arrives. **Every GX state setter calls it first**,
  so state changes always flush the open batch before mutating state.
- **Draw-call merging (2026-07-13)**: `GXBegin` appends to the still-open
  previous batch when it completed with `dirty == 0` (nothing ran in between),
  same primitive (QUADS/TRIANGLES only) and same vtxfmt. Strips/fans never
  merge (would bridge geometry). See GXBegin in pc_gx.c.
- **State-set dedup (2026-07-13)**: setters early-return on no-op re-sets
  (compare before flush/DIRTY) so merging spans them — `pc_gx_state_dedup`,
  env PC_NO_STATE_DEDUP=1 disables. Rules when touching setters: any setter
  that makes a GL call (viewport/scissor/GXLoadTexObj) must flush BEFORE its
  first GL call and may only skip when the GL-side state is provably
  identical (viewport/scissor shadow computed GL values, invalidated in
  begin_frame/restore_after_nes; GXLoadTexObj compares g_gx.gl_textures[map]
  + sampler state). Pure-state setters just compare their g_gx fields.
- Quads draw via a static quad→triangle index buffer (GL_ELEMENT_ARRAY);
  everything else via glDrawArrays. One VAO/VBO bound forever.
- **Streaming VBO (2026-07-13, v2)**: 6MB buffer (PC_GX_STREAM_VERTS=128k),
  batches append at a running offset, orphan only on wrap. Upload via
  glMapBufferRange(WRITE|INVALIDATE_RANGE|UNSYNCHRONIZED) with SubData
  fallback (PC_STREAM_SUBDATA=1 forces it). Draws use
  glDrawElementsBaseVertex / glDrawArrays(first) so attrib pointers are
  NEVER respecified on GLES3.2 drivers; pre-3.2 falls back to
  `pc_gx_set_attrib_base` rebasing. First 2000 flushes poll glGetError
  and auto-drop to the legacy per-flush-orphan path (with log line) on
  any error. WARNING: v1 (SubData + per-batch attrib respec) rendered
  fine on Mesa but SILENTLY black-screened the Mali blob — do not
  reintroduce per-draw glVertexAttribPointer churn. Anything adding draw
  paths must pass draw_base through the same mechanism.
  PC_NO_STREAM_VBO=1 forces legacy. Device-verified but throughput-
  neutral (~30µs/draw is blob dispatch overhead) — reduce draw count
  instead (kb/perf.md #13).

## Shader system details

- Uniforms uploaded per dirty group (PC_GX_DIRTY_* bits). **Uniform value
  shadowing (2026-07-13)**: shader switch does NOT set dirty=ALL — uniform
  groups (bits 0-11) carry generation counters bumped on change
  (`pc_gx_mark_dirty`); each program records last-uploaded gens (tev cache
  entry `gens[]`, exposed via `pc_gx_tev_last_gens`), flush uploads only
  mismatches. GL-state groups (bits 12-15) stay on `g_gx.dirty`. Anything
  touching GL outside the GX layer must call `pc_gx_dirty_all()` (bumps all
  gens), not just set dirty=ALL. TEXTURES+TEV_STAGES upload/record as one
  block. Env PC_NO_UNIFORM_SHADOW=1 disables.
- **Per-program uniform locations**: `PCGXUniformLocs` filled once at link
  (`pc_gx_fill_uniform_locations` in pc_gx.c), stored in the shader cache
  entry; `pc_gx_tev_last_locs` exposes the last-returned program's locs and
  pc_gx.c struct-copies it on switch. Never call glGetUniformLocation per
  switch (~150 calls — was a real cost).
- **Shader binary disk cache** (`shader_cache.bin`, cwd; GLES builds only,
  `SHADER_DISK_CACHE` define): header magic `ACSC` + version + driver hash
  (FNV of GL_RENDERER+GL_VERSION — driver update invalidates). Entries:
  ShaderKey + binfmt + len + blob, appended on each new compile
  (`sdc_append`), all preloaded at boot (`sdc_load`) via glProgramBinary.
  `GL_PROGRAM_BINARY_RETRIEVABLE_HINT` set before link. Env
  `PC_NO_SHADER_CACHE=1` disables. Fallback uber-shader kept for
  compile-failure path (its locs in `s_fallback_locs`).
- Changing generated shader source or ShaderKey layout? **Bump SDC_VERSION**
  so stale caches self-invalidate.

## Texture system details

- Cache key: data_ptr + w/h + format + TLUT identity + content hashes
  (FNV of head+tail bytes — game reuses buffers with different contents).
- **4096-slot open-addressed hash index** over the entry array (2026-07-13);
  bulk-only deletion: eviction (oldest half, memmove) rebuilds the index,
  invalidate clears it. Never delete single entries without handling index.
- **Per-frame decode budget**: max 8 non-tiny (>32x32) CPU decodes per frame
  on the miss path; over budget → shared 1x1 white placeholder bound, entry
  NOT cached, retried next frame. Texture-pack and EFB-capture paths exempt.
- **NEON decode paths** (`__ARM_NEON` guarded) for the hot formats + u32
  palette writes; scalar fallbacks kept for other archs and edge blocks.
  Output must stay byte-identical to scalar.
- EFB captures (GXCopyTex) stay GPU-side: blit into a texture via FBO
  (`pc_gx_efb_capture_*`), no glReadPixels round trip (PC_ENHANCEMENTS).

## GLES 2.0 path (Miyoo A30 / Mali-400, 2026-10-07 — not device-verified)

- Context loop in pc_main.c: GLES 3.1 → 2.0 → 2.0 with 16-bit depth
  (window recreated per attempt: EGL config depends on the attributes).
  `PC_GLES2=1` starts at 2.0 AND forces the GLES2 code path on a 3.x
  context, hiding 3.x-only entry points — that's how to exercise it in the
  Mesa smoke harness (`PC_GLES2_NO_VAO=1` also drops VAOs).
- `pc_gl_compat.c`: `g_pc_gles2`, OES aliasing (VAO, program binary, base
  vertex), no-op VAO stubs when there's no OES_vertex_array_object
  (`g_pc_has_vao=0`: anything drawing with its own attribs must leave via
  `pc_gx_rebind_vertex_state()` — the overlay and present quad do),
  `g_pc_has_npot` (NPOT + REPEAT → CLAMP in `gx_gl_wrap`).
- Shaders: `generate_frag` emits GLSL ES 1.00 when `g_pc_gles2` (varying,
  texture2D, gl_FragColor via #define, mediump int). Swap tables use
  one-hot dot products instead of `v[i]` / `u_swap_table[i]`. Per-stage
  texcoord selection moved into `shaders/default_es2.vert` (outputs
  v_stc0..2) so texture2D gets untouched varyings — Mali-400 fragment math
  is fp16 but direct varying lookups aren't. Keep default_es2.vert's math
  in sync with default.vert. GLES3 output is byte-identical to before
  (checked over all 101 seed keys) — no SDC_VERSION bump was needed.
  No uber-shader fallback on GLES2.
- Present pass: when `pc_gl_present_needed()` (GLES2 or rotation≠0) the
  game always renders into s_render_fbo; `pc_gx_blit_to_screen` draws it as
  a (rotated) quad via `pc_gl_draw_tex_quad`, which saves/restores all GL
  state GX caches. The overlay draws straight onto the panel afterwards,
  rotating its clip-space verts (`ov_flush_verts`) — no extra full-screen
  pass. g_pc_window_w/h are LOGICAL (as-viewed) dims; g_pc_phys_w/h are
  the drawable.
- EFB copies without glBlitFramebuffer: quad from s_render_tex into the
  capture texture with flipped v. Fixed in passing (all GL flavours): the
  blit path used to read FB 0 and leave DRAW_FRAMEBUFFER=0 even when the
  game renders into the render-scale FBO (render_scale < 100 lost
  everything drawn after a GXCopyTex); it now reads/rebinds the game's
  actual target (`pc_gx_game_fbo`).
- **GLES2 audit (2026-10-07, after the A30 played correctly)**:
  - Done: identity-swap specialization. AC only ever selects TEV swap
    table 0 (identity; emu64.c sets tables 1-3 but no stage uses them), so
    GLES2 keys carry `ShaderKey._pad` bit0 = "a stage really swaps" and the
    common shader drops applySwap (~25 fp16 ALU ops per stage per pixel).
    Seed keys have _pad=0 → boot warm-up compiles exactly the no-swap
    variants. GLES3 keys/output unchanged (verified byte-identical, 101
    keys). GLES2 disk-cache hash salt bumped 0x32→0x33.
  - Done: discard render-FBO depth at frame end (EXT_discard_framebuffer
    on GLES2, glInvalidateFramebuffer on GLES3) — saves the depth tile
    write-back on tile-based Malis.
  - Done earlier: matrices CPU-transposed (GL_TRUE transpose is invalid on
    GLES2), legacy per-flush VBO on GLES2.
  - Checked, fine: no GX_POINTS anywhere (lines only in NES/debug draws);
    PC_GL_CHECK compiled out; no glGet in per-draw paths; NPOT + depth24
    supported on the A30 Mali-400.
  - Done: compact texture upload (`gx_upload_rgba` in pc_gx_texture.c,
    default on for GLES2, `PC_TEX16=0/1` overrides). Content-based, after
    the (unchanged, byte-identical) RGBA8 decode: grey → LUMINANCE /
    LUMINANCE_ALPHA, else exact 565 / 5551 / 4444, else RGBA8. "Exact" =
    every byte is producible by the decoders' own truncating expansion
    (v*255/(2^n-1)) — inverse tables s_inv5/6/4 — so the GPU samples
    within 1/255 of the old upload. Unit-tested per GX format
    (/tmp harness: I4/I8/IA4/IA8 → LA8 lossless; RGB565, opaque RGB5A3,
    RGB565/IA8/opaque-RGB5A3 palettes → 16-bit; translucent RGB5A3,
    RGBA8, CMPR stay RGBA8). Verbose log: `[PC/TEX] compact upload: ...`
    at 16/32/64/128 then every 256 uploads. NOTE for host-side tests:
    pc_types.h `u32` is `unsigned long` → 8 bytes on 64-bit hosts, which
    breaks decode_CI8's u32 palette copy there (device is fine).
    Possible next step (lossy, not done): translucent RGB5A3 → 4444.
  - Open ideas (not done): reuse EFB-capture textures instead of
    glGenTextures per GXCopyTex; texture cache is capped by count (2048),
    not bytes — watch memory on 512 MB devices; analog stick via spruce's
    virtual pad.

## Frame flow

`VIWaitForRetrace` (pc_vi.c): poll events → (frameskip fast path) → blit
render-FBO to screen → overlay → swap → dynamic-fps EMA update →
frame pacing wait → begin next frame. Render-scale renders into
`s_render_fbo` at reduced res, upscaled at blit. `fps_target=6` in
settings.ini means **dynamic**, not 6 fps.
