/* pc_gl_compat.c - GLES 2.0 fallback + display rotation (see pc_gl_compat.h) */
#include "pc_gl_compat.h"

int g_pc_gles2 = 0;
int g_pc_has_vao = 1;
int g_pc_has_npot = 1;
int g_pc_rotation = 0;
int g_pc_phys_w = PC_SCREEN_WIDTH;
int g_pc_phys_h = PC_SCREEN_HEIGHT;

/* --- VAO stubs: GLES 2.0 without OES_vertex_array_object. Vertex attrib
 * state is then global, so every component that draws re-specifies its
 * own attribs and pc_gx_rebind_vertex_state() puts GX's back. --- */
static void GLAD_API_PTR stub_gen_vaos(GLsizei n, GLuint* arrays) {
    for (GLsizei i = 0; i < n; i++) arrays[i] = 1;
}
static void GLAD_API_PTR stub_bind_vao(GLuint array) { (void)array; }
static void GLAD_API_PTR stub_delete_vaos(GLsizei n, const GLuint* arrays) {
    (void)n; (void)arrays;
}

static int has_ext_in_list(const char* list, const char* name) {
    size_t n = strlen(name);
    const char* p = list;
    while (p && (p = strstr(p, name)) != NULL) {
        if ((p == list || p[-1] == ' ') && (p[n] == ' ' || p[n] == '\0'))
            return 1;
        p += n;
    }
    return 0;
}

void pc_gl_compat_init(void) {
#ifdef PC_USE_GLES
    /* PC_GLES2=1 forces the GLES2 path even when the driver handed back a
     * 3.x context (Mesa does for a 2.0 request) — that's how the Docker smoke
     * harness exercises it. The 3.x-only entry points are hidden so the same
     * fallbacks run as on a real 2.0 driver; PC_GLES2_NO_VAO=1 additionally
     * drops VAOs (Mali-400 may not expose OES_vertex_array_object). */
    const char* force = getenv("PC_GLES2");
    g_pc_gles2 = !GLAD_GL_ES_VERSION_3_0 || (force && atoi(force) != 0);
    if (g_pc_gles2) {
        const char* exts = (const char*)glGetString(GL_EXTENSIONS);
        if (!exts) exts = "";

        if (GLAD_GL_ES_VERSION_3_0) {
            fprintf(stderr, "[PC] PC_GLES2: emulating GLES 2.0 on a 3.x context\n");
            glad_glMapBufferRange = NULL;
            glad_glUnmapBuffer = NULL;
            glad_glDrawElementsBaseVertex = NULL;
            glad_glBlitFramebuffer = NULL;
            glad_glGetStringi = NULL;
            glad_glProgramBinary = NULL;
            glad_glGetProgramBinary = NULL;
            glad_glProgramParameteri = NULL;
            if (getenv("PC_GLES2_NO_VAO"))
                GLAD_GL_OES_vertex_array_object = 0;
        }

        if (GLAD_GL_OES_vertex_array_object && glad_glBindVertexArrayOES) {
            glad_glGenVertexArrays = glad_glGenVertexArraysOES;
            glad_glBindVertexArray = glad_glBindVertexArrayOES;
            glad_glDeleteVertexArrays = glad_glDeleteVertexArraysOES;
            g_pc_has_vao = 1;
        } else {
            glad_glGenVertexArrays = stub_gen_vaos;
            glad_glBindVertexArray = stub_bind_vao;
            glad_glDeleteVertexArrays = stub_delete_vaos;
            g_pc_has_vao = 0;
        }
        /* GL_PROGRAM_BINARY_LENGTH(_OES) share an enum value, so the disk
         * cache works unchanged on top of the OES entry points. */
        if (glad_glProgramBinaryOES && glad_glGetProgramBinaryOES &&
            has_ext_in_list(exts, "GL_OES_get_program_binary")) {
            glad_glProgramBinary = glad_glProgramBinaryOES;
            glad_glGetProgramBinary = glad_glGetProgramBinaryOES;
        }
        if (glad_glDrawElementsBaseVertexOES &&
            has_ext_in_list(exts, "GL_OES_draw_elements_base_vertex"))
            glad_glDrawElementsBaseVertex = glad_glDrawElementsBaseVertexOES;

        g_pc_has_npot = has_ext_in_list(exts, "GL_OES_texture_npot") ||
                        has_ext_in_list(exts, "GL_ARB_texture_non_power_of_two");

        fprintf(stderr, "[PC] GLES 2.0 path: vao=%s program_binary=%s base_vertex=%s npot=%d\n",
                g_pc_has_vao ? "OES" : "none",
                glProgramBinary ? "OES" : "none",
                glDrawElementsBaseVertex ? "OES" : "none",
                g_pc_has_npot);
        fprintf(stderr, "[PC] GL extensions: %s\n", exts);
    }
#endif
}

void pc_gl_compat_pick_rotation(int phys_w, int phys_h, int fullscreen) {
    const char* env = getenv("PC_ROTATE");
    int rot;
    if (env && *env) {
        rot = atoi(env);
    } else {
        /* Auto: landscape game on a portrait-scanned panel (A30: 480x640). */
        rot = (fullscreen && phys_h > phys_w) ? 90 : 0;
    }
    rot = ((rot % 360) + 360) % 360;
    if (rot != 0 && rot != 90 && rot != 180 && rot != 270) rot = 0;
    if (rot != g_pc_rotation)
        fprintf(stderr, "[PC] Display rotation: %d (panel %dx%d)\n", rot, phys_w, phys_h);
    g_pc_rotation = rot;
}

int pc_gl_present_needed(void) {
    return g_pc_rotation != 0 || g_pc_gles2;
}

void pc_gl_rotate_clip(float* x, float* y) {
    float ox = *x, oy = *y;
    switch (g_pc_rotation) {
        case 90:  *x = -oy; *y =  ox; break;
        case 180: *x = -ox; *y = -oy; break;
        case 270: *x =  oy; *y = -ox; break;
        default: break;
    }
}

void pc_gl_bind_screen(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pc_phys_w, g_pc_phys_h);
}

/* --- Textured quad (present / render-scale upscale / GLES2 EFB copy) --- */

static GLuint s_q_prog = 0;
static GLuint s_q_vbo = 0;
static GLuint s_q_vao = 0;
static GLint  s_q_loc_tex = -1;

static GLuint q_compile(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(sh, sizeof(log), NULL, log);
        fprintf(stderr, "[PC] present shader compile error: %s\n", log);
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

static int q_init(void) {
    if (s_q_prog) return 1;

    const char* vs_src;
    const char* fs_src;
#ifdef PC_USE_GLES
    if (g_pc_gles2) {
        vs_src =
            "#version 100\n"
            "attribute vec2 a_pos; attribute vec2 a_uv; varying vec2 v_uv;\n"
            "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;}\n";
        fs_src =
            "#version 100\n"
            "precision mediump float;\n"
            "varying vec2 v_uv; uniform sampler2D u_tex;\n"
            "void main(){gl_FragColor=vec4(texture2D(u_tex,v_uv).rgb,1.0);}\n";
    } else {
        vs_src =
            "#version 300 es\n"
            "layout(location=0) in vec2 a_pos; layout(location=1) in vec2 a_uv; out vec2 v_uv;\n"
            "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;}\n";
        fs_src =
            "#version 300 es\n"
            "precision mediump float;\n"
            "in vec2 v_uv; uniform sampler2D u_tex; out vec4 o;\n"
            "void main(){o=vec4(texture(u_tex,v_uv).rgb,1.0);}\n";
    }
#else
    vs_src =
        "#version 330 core\n"
        "layout(location=0) in vec2 a_pos; layout(location=1) in vec2 a_uv; out vec2 v_uv;\n"
        "void main(){gl_Position=vec4(a_pos,0.0,1.0);v_uv=a_uv;}\n";
    fs_src =
        "#version 330 core\n"
        "in vec2 v_uv; uniform sampler2D u_tex; out vec4 o;\n"
        "void main(){o=vec4(texture(u_tex,v_uv).rgb,1.0);}\n";
#endif

    GLuint vs = q_compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs = q_compile(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glBindAttribLocation(prog, 0, "a_pos");
    glBindAttribLocation(prog, 1, "a_uv");
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "[PC] present program link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }
    s_q_prog = prog;
    s_q_loc_tex = glGetUniformLocation(prog, "u_tex");

    glGenBuffers(1, &s_q_vbo);
    if (g_pc_has_vao) {
        GLint prev_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
        glGenVertexArrays(1, &s_q_vao);
        glBindVertexArray(s_q_vao);
        glBindBuffer(GL_ARRAY_BUFFER, s_q_vbo);
        glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray((GLuint)prev_vao);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, s_q_vbo);
        glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_STREAM_DRAW);
    }
    return 1;
}

void pc_gl_draw_tex_quad(GLuint tex, float x, float y, float w, float h,
                         float target_w, float target_h,
                         float u0, float v0, float u1, float v1,
                         int rotate) {
    if (!q_init()) return;

    /* Everything GX caches/dedups GL-side is saved and restored, so this
     * can run mid-frame (GLES2 EFB copies) without GX noticing. */
    GLint sv_prog, sv_active, sv_tex0;
    GLboolean sv_blend = glIsEnabled(GL_BLEND);
    GLboolean sv_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean sv_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean sv_cull = glIsEnabled(GL_CULL_FACE);
    GLboolean sv_cmask[4];
    glGetIntegerv(GL_CURRENT_PROGRAM, &sv_prog);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &sv_active);
    glGetBooleanv(GL_COLOR_WRITEMASK, sv_cmask);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &sv_tex0);

    float x0 = x / target_w * 2.0f - 1.0f;
    float x1 = (x + w) / target_w * 2.0f - 1.0f;
    float y0 = y / target_h * 2.0f - 1.0f;
    float y1 = (y + h) / target_h * 2.0f - 1.0f;
    float v[16] = {
        x0, y0, u0, v0,
        x1, y0, u1, v0,
        x0, y1, u0, v1,
        x1, y1, u1, v1,
    };
    if (rotate) {
        for (int i = 0; i < 4; i++)
            pc_gl_rotate_clip(&v[i * 4 + 0], &v[i * 4 + 1]);
    }

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(s_q_prog);
    glBindTexture(GL_TEXTURE_2D, tex);
    if (s_q_loc_tex >= 0) glUniform1i(s_q_loc_tex, 0);

    if (g_pc_has_vao) glBindVertexArray(s_q_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_q_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    if (!g_pc_has_vao) {
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glDisableVertexAttribArray(2);
        glDisableVertexAttribArray(3);
    }
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_2D, (GLuint)sv_tex0);
    glActiveTexture((GLenum)sv_active);
    glUseProgram((GLuint)sv_prog);
    glColorMask(sv_cmask[0], sv_cmask[1], sv_cmask[2], sv_cmask[3]);
    if (sv_blend) glEnable(GL_BLEND);
    if (sv_depth) glEnable(GL_DEPTH_TEST);
    if (sv_scissor) glEnable(GL_SCISSOR_TEST);
    if (sv_cull) glEnable(GL_CULL_FACE);
    pc_gx_rebind_vertex_state();
}

void pc_gl_compat_shutdown(void) {
    if (s_q_prog) { glDeleteProgram(s_q_prog); s_q_prog = 0; }
    if (s_q_vbo)  { glDeleteBuffers(1, &s_q_vbo); s_q_vbo = 0; }
    if (s_q_vao && g_pc_has_vao) { glDeleteVertexArrays(1, &s_q_vao); }
    s_q_vao = 0;
}
