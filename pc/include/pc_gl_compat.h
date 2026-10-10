/* pc_gl_compat.h - GLES 2.0 fallback + display rotation support
 *
 * The renderer targets GLES 3.x (Mali-G31 on H700 handhelds). Mali-400
 * devices (Miyoo A30) only have GLES 2.0, and the A30 panel is a portrait
 * 480x640 scanned display that apps must rotate themselves. This layer:
 *   - records which GL flavour the context actually is (g_pc_gles2)
 *   - aliases OES extension entry points onto the core-3.x pointers the
 *     renderer calls (VAO, program binary, base vertex) when available
 *   - installs no-op VAO stubs when there are no VAOs at all, in which case
 *     code that switches vertex layouts must re-specify its attribs
 *   - tracks the physical drawable vs. the logical (landscape) screen and
 *     the rotation applied by the final present pass */
#ifndef PC_GL_COMPAT_H
#define PC_GL_COMPAT_H

#include "pc_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int g_pc_gles2;       /* context is GLES 2.0 (GLSL ES 1.00 shaders) */
extern int g_pc_has_vao;     /* real vertex array objects (core or OES) */
extern int g_pc_has_npot;    /* NPOT textures support REPEAT/MIRROR wrap */
extern int g_pc_rotation;    /* display rotation, degrees CCW: 0/90/180/270 */
extern int g_pc_phys_w;      /* drawable size in panel pixels (unrotated) */
extern int g_pc_phys_h;

/* Call once after glad has loaded the context's entry points. */
void pc_gl_compat_init(void);

/* Picks g_pc_rotation from PC_ROTATE (0/90/180/270) or, unset, auto:
 * a fullscreen portrait drawable means a landscape game on a portrait
 * panel → 90. Call before deriving the logical window size. */
void pc_gl_compat_pick_rotation(int phys_w, int phys_h, int fullscreen);

/* 1 when frames must go through the offscreen render target + present
 * pass instead of drawing straight into the default framebuffer. */
int pc_gl_present_needed(void);

/* Rotate a clip-space position by g_pc_rotation (in place). */
void pc_gl_rotate_clip(float* x, float* y);

/* Bind the default framebuffer with a viewport covering the whole panel. */
void pc_gl_bind_screen(void);

/* Draw `tex` as a quad into the logical-screen rect (x,y,w,h; GL-style
 * bottom-left origin) of the currently bound target. `rotate` applies the
 * display rotation (final present); `flip_y` mirrors the source
 * vertically; u0..v1 select the source sub-rect in texture coordinates.
 * Clobbers program, texture unit 0, vertex attribs, blend/depth/scissor;
 * callers restore what they need (pc_gx_rebind_vertex_state for GX). */
void pc_gl_draw_tex_quad(GLuint tex, float x, float y, float w, float h,
                         float target_w, float target_h,
                         float u0, float v0, float u1, float v1,
                         int rotate);

void pc_gl_compat_shutdown(void);

/* pc_gx.c: re-establish GX's VBO/EBO/attrib pointers after another
 * renderer component changed them (needed when there are no VAOs). */
void pc_gx_rebind_vertex_state(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_GL_COMPAT_H */
