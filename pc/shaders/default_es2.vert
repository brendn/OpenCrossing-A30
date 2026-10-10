/* GLSL ES 1.00 port of default.vert for GLES 2.0 GPUs (Mali-400 / Miyoo A30).
 * Keep the math in sync with default.vert. Differences:
 *  - attribute/varying, no layout(location) (pc_gx_tev binds locations)
 *  - no bitwise ops: the light mask is peeled with mod/floor
 *  - per-TEV-stage texcoord selection (u_tevN_tc_src) happens here, so the
 *    fragment shader feeds untouched varyings straight into texture2D —
 *    Mali-400 fragment math is fp16, but direct varying lookups aren't. */
attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec4 a_color0;
attribute vec2 a_texcoord0;
uniform mat4 u_projection;
uniform mat4 u_modelview;
uniform mat3 u_normal_mtx;
uniform vec4 u_texmtx_row0;
uniform vec4 u_texmtx_row1;
uniform int u_texmtx_enable;
uniform vec4 u_texmtx1_row0;
uniform vec4 u_texmtx1_row1;
uniform int u_texmtx1_enable;
uniform int u_texgen_src0;  /* 1=GX_TG_NRM, 4=GX_TG_TEX0, etc. */
uniform int u_texgen_src1;
uniform int u_tev0_tc_src;
uniform int u_tev1_tc_src;
uniform int u_tev2_tc_src;
/* Per-vertex GX lighting */
uniform int u_lighting_enabled;
uniform int u_light_mask;
uniform vec3 u_light_pos[8];
uniform vec4 u_light_color[8];
uniform int u_diff_fn;
uniform int u_attn_fn;
uniform vec3 u_light_dir[8];
uniform vec3 u_light_a[8];  /* angular attenuation: a0, a1, a2 */
uniform vec3 u_light_k[8];  /* distance attenuation: k0, k1, k2 */
varying vec4 v_color;
varying vec2 v_stc0;
varying vec2 v_stc1;
varying vec2 v_stc2;
varying float v_fog_z;
varying vec3 v_light_accum;  /* per-vertex light contributions (ambient added in frag) */
void main() {
    vec4 eyePos = u_modelview * vec4(a_position, 1.0);
    gl_Position = u_projection * eyePos;
    v_fog_z = -eyePos.z;
    v_color = a_color0;
    vec3 N = normalize(u_normal_mtx * a_normal);
    /* Compute per-vertex light contributions */
    v_light_accum = vec3(0.0);
    if (u_lighting_enabled != 0 && u_light_mask != 0) {
        float mask = float(u_light_mask);
        for (int i = 0; i < 8; i++) {
            float bit = mod(mask, 2.0);
            mask = floor(mask * 0.5);
            if (bit > 0.5) {
                vec3 lv = u_light_pos[i] - eyePos.xyz;
                float d = length(lv);
                vec3 L = (d > 0.0001) ? lv / d : vec3(0.0, 0.0, 1.0);
                /* Diffuse: GX_DF_NONE=0, GX_DF_SIGN=1, GX_DF_CLAMP=2 */
                float diff = 1.0;
                if (u_diff_fn == 2)      diff = max(0.0, dot(N, L));
                else if (u_diff_fn == 1) diff = dot(N, L);
                /* Attenuation: GX_AF_SPOT=2 */
                float atten = 1.0;
                if (u_attn_fn == 2) {
                    vec3 ld = u_light_dir[i];
                    float ld_len = length(ld);
                    if (ld_len > 0.0001) {
                        float ca = dot(L, ld / ld_len);
                        float spot = u_light_a[i].x + u_light_a[i].y * ca + u_light_a[i].z * ca * ca;
                        float dk = u_light_k[i].x + u_light_k[i].y * d + u_light_k[i].z * d * d;
                        atten = (dk > 0.0001) ? clamp(spot / dk, 0.0, 1.0) : 1.0;
                    }
                }
                v_light_accum += u_light_color[i].rgb * diff * atten;
            }
        }
    }
    vec2 tc0, tc1;
    vec4 src0 = (u_texgen_src0 == 1) ? vec4(N, 1.0) : vec4(a_texcoord0, 0.0, 1.0);
    if (u_texmtx_enable != 0) {
        tc0 = vec2(dot(u_texmtx_row0, src0), dot(u_texmtx_row1, src0));
    } else {
        tc0 = src0.xy;
    }
    vec4 src1 = (u_texgen_src1 == 1) ? vec4(N, 1.0) : vec4(a_texcoord0, 0.0, 1.0);
    if (u_texmtx1_enable != 0) {
        tc1 = vec2(dot(u_texmtx1_row0, src1), dot(u_texmtx1_row1, src1));
    } else {
        tc1 = src1.xy;
    }
    v_stc0 = (u_tev0_tc_src == 0) ? tc0 : tc1;
    v_stc1 = (u_tev1_tc_src == 0) ? tc0 : tc1;
    v_stc2 = (u_tev2_tc_src == 0) ? tc0 : tc1;
}
