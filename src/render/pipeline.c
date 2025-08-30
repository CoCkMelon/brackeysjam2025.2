#include "pipeline.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ame/camera.h"

// Parallax tuning (higher K => stronger reduction of movement with distance)
#ifndef PARALLAX_K
#define PARALLAX_K 0.01f
#endif

// 3-Pass Pipeline Implementation:
// Pass 1: Sprites (batched by texture, full resolution)
// Pass 2: Meshes (rendered to offscreen texture, supersampled)
// Pass 3: Composite (downscale mesh texture to screen)

// Sprite shader
static const char* SPRITE_VS =
    "#version 450 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "layout(location=3) in float a_par;\n"
    "uniform vec2 u_res;\n"
    "uniform vec4 u_cam; // x,y,zoom,rot\n"
    "out vec4 v_col;\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p = a_pos - u_cam.xy * a_par;\n"
    "  p *= u_cam.z;\n"
    "  vec2 ndc = vec2((p.x/u_res.x)*2.0 - 1.0, (p.y/u_res.y)*2.0 - 1.0);\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "  v_col = a_col;\n"
    "  v_uv = vec2(a_uv.x, 1.0 - a_uv.y);\n"
    "}\n";

static const char* SPRITE_FS =
    "#version 450 core\n"
    "in vec4 v_col;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main(){\n"
    "  frag = texture(u_tex, v_uv) * v_col;\n"
    "}\n";

// Mesh shader (same as sprite for now, could add parallax later)
static const char* MESH_VS =
    "#version 450 core\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_col;\n"
    "layout(location=3) in float a_par;\n"
    "uniform vec2 u_res;\n"
    "uniform vec4 u_cam; // x,y,zoom,rot\n"
    "out vec4 v_col;\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  vec2 p = a_pos - u_cam.xy * a_par;\n"
    "  p *= u_cam.z;\n"
    "  vec2 ndc = vec2((p.x/u_res.x)*2.0 - 1.0, (p.y/u_res.y)*2.0 - 1.0);\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "  v_col = a_col;\n"
    "  v_uv = vec2(a_uv.x, 1.0 - a_uv.y);\n"
    "}\n";

static const char* MESH_FS =
    "#version 450 core\n"
    "in vec4 v_col;\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main(){\n"
    "  frag = texture(u_tex, v_uv) * v_col;\n"
    "}\n";

// Composite shader for fullscreen quad
static const char* COMP_VS =
    "#version 450 core\n"
    "out vec2 v_uv;\n"
    "void main(){\n"
    "  // Fullscreen triangle trick\n"
    "  vec2 pos;\n"
    "  if (gl_VertexID == 0) { pos = vec2(-1.0, -1.0); v_uv = vec2(0.0, 0.0); }\n"
    "  else if (gl_VertexID == 1) { pos = vec2( 3.0, -1.0); v_uv = vec2(2.0, 0.0); }\n"
    "  else { pos = vec2(-1.0,  3.0); v_uv = vec2(0.0, 2.0); }\n"
    "  gl_Position = vec4(pos, 0.0, 1.0);\n"
    "}\n";

static const char* COMP_FS =
    "#version 450 core\n"
    "in vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main(){ frag = texture(u_tex, v_uv); }\n";

// Fullscreen snow shader (pixelated, camera + wind influenced, branchless)
static const char* SNOW_FS =
    "#version 450 core\n"
    "in vec2 v_uv;\n"
    "out vec4 frag;\n"
    "uniform vec2 u_viewport;        // viewport size in pixels\n"
    "uniform float u_time;           // seconds\n"
    "uniform vec2 u_cam;             // camera x,y (world units)\n"
    "uniform vec2 u_wind;            // wind velocity in pixels/sec (x,y)\n"
    "uniform float u_density;        // 0..1 density\n"
    "uniform float u_pixel_scale;    // pixelation scale (e.g., 4)\n"
    "\n"
    "float hash12(vec2 p){\n"
    "  vec3 p3 = fract(vec3(p.xyx) * 0.1031);\n"
    "  p3 += dot(p3, p3.yzx + 33.33);\n"
    "  return fract((p3.x + p3.y) * p3.z);\n"
    "}\n"
    "\n"
    "// Simple plus-shaped snowflake (clear and recognizable)\n"
    "float flake_shape(vec2 q){\n"
    "  // Create a simple + shape with dots\n"
    "  float d = 100.0;\n"
    "  // Horizontal bar\n"
    "  d = min(d, max(abs(q.y) - 0.08, abs(q.x) - 0.6));\n"
    "  // Vertical bar\n"
    "  d = min(d, max(abs(q.x) - 0.08, abs(q.y) - 0.6));\n"
    "  // Diagonal X bars (thinner)\n"
    "  vec2 q1 = abs(q);\n"
    "  d = min(d, abs(q1.x - q1.y) - 0.06);\n"
    "  d = min(d, abs(q1.x + q1.y - 0.85) - 0.06);\n"
    "  // Center dot\n"
    "  d = min(d, length(q) - 0.15);\n"
    "  // Corner dots\n"
    "  d = min(d, length(q - vec2(0.5, 0.0)) - 0.1);\n"
    "  d = min(d, length(q - vec2(-0.5, 0.0)) - 0.1);\n"
    "  d = min(d, length(q - vec2(0.0, 0.5)) - 0.1);\n"
    "  d = min(d, length(q - vec2(0.0, -0.5)) - 0.1);\n"
    "  return 1.0 - smoothstep(0.0, 0.02, d);\n"
    "}\n"
    "\n"
    "vec2 rot2(vec2 v, float a){ float s = sin(a), c = cos(a); return mat2(c,-s,s,c)*v; }\n"
    "\n"
    "void main(){\n"
    "  // Pixelate coordinates\n"
    "  vec2 pix = u_viewport / u_pixel_scale;\n"
    "  vec2 uv_px = floor(v_uv * pix) / pix;\n"
    "  vec2 p = uv_px * u_viewport; // pixel coords\n"
    "\n"
    "  vec2 cam_off = u_cam * 0.5;  // reduce camera influence\n"
    "  vec2 wind_off = u_wind * u_time;\n"
    "\n"
    "  // Single layer for clarity (was too noisy with 3)\n"
    "  vec2 w = p + cam_off + wind_off;\n"
    "  float cellsize = 80.0;  // much larger cells\n"
    "  vec2 cell = floor(w / cellsize);\n"
    "  vec2 celluv = fract(w / cellsize);\n"
    "  \n"
    "  float rnd = hash12(cell + vec2(13.0, 7.0));\n"
    "  \n"
    "  // Only spawn in some cells\n"
    "  float spawn = step(rnd, u_density);\n"
    "  \n"
    "  // Position within cell (randomized but centered)\n"
    "  vec2 center = vec2(0.5 + (hash12(cell + vec2(23.0, 11.0)) - 0.5) * 0.3,\n"
    "                     0.5 + (hash12(cell + vec2(31.0, 17.0)) - 0.5) * 0.3);\n"
    "  \n"
    "  // Local space coords\n"
    "  vec2 local = (celluv - center) * 3.0;  // scale up the flake\n"
    "  \n"
    "  // Very slow rotation\n"
    "  float rot_speed = 0.1 + rnd * 0.2;\n"
    "  vec2 q = rot2(local, u_time * rot_speed);\n"
    "  \n"
    "  // Get flake shape\n"
    "  float shape = flake_shape(q) * spawn;\n"
    "  \n"
    "  // Second layer (optional, farther)\n"
    "  vec2 w2 = p + cam_off * 0.3 + wind_off * 0.6;\n"
    "  float cellsize2 = 120.0;\n"
    "  vec2 cell2 = floor(w2 / cellsize2);\n"
    "  vec2 celluv2 = fract(w2 / cellsize2);\n"
    "  float rnd2 = hash12(cell2 + vec2(53.0, 29.0));\n"
    "  float spawn2 = step(rnd2, u_density * 0.7);\n"
    "  vec2 center2 = vec2(0.5 + (hash12(cell2 + vec2(43.0, 19.0)) - 0.5) * 0.3,\n"
    "                      0.5 + (hash12(cell2 + vec2(47.0, 23.0)) - 0.5) * 0.3);\n"
    "  vec2 local2 = (celluv2 - center2) * 3.5;\n"
    "  float rot_speed2 = 0.08 + rnd2 * 0.15;\n"
    "  vec2 q2 = rot2(local2, u_time * rot_speed2);\n"
    "  float shape2 = flake_shape(q2 * 1.2) * spawn2 * 0.6;  // smaller, dimmer\n"
    "  \n"
    "  float alpha = clamp(shape + shape2, 0.0, 1.0);\n"
    "  vec3 col = vec3(0.98, 0.99, 1.0);  // nearly white\n"
    "  frag = vec4(col, alpha * 0.9);\n"
    "}\n";

// Vertex format
typedef struct {
    float x, y, u, v, r, g, b, a, par;
} Vtx;

// Triangle with depth for sorting
typedef struct {
    Vtx verts[3];
    float depth;  // Average Z of the triangle for sorting
} Triangle;

// Sprite batch (grouped by texture)
typedef struct {
    GLuint texture;
    Vtx* vertices;
    size_t count;
    size_t capacity;
} SpriteBatch;

// Mesh batch
typedef struct {
    const AmeLocalMesh* mesh;
    float tx, ty, tz;  // object translation xyz
    float sx, sy, sz;  // object scale xyz
    float r, g, b, a;  // color
    float depth;       // calculated depth for sorting (higher values render behind)
} MeshBatch;

// Pipeline state
static struct {
    // Shaders
    GLuint sprite_prog, mesh_prog, comp_prog, snow_prog;
    GLint sprite_u_res, sprite_u_cam, sprite_u_tex;
    GLint mesh_u_res, mesh_u_cam, mesh_u_tex;
    GLint comp_u_tex;
    // Snow uniforms
    GLint snow_u_viewport, snow_u_time, snow_u_cam, snow_u_wind, snow_u_density, snow_u_pixel_scale;

    // VAOs
    GLuint sprite_vao, sprite_vbo;
    GLuint mesh_vao, mesh_vbo;
    GLuint comp_vao;

    // Framebuffers (Pass 2: mesh to texture, Pass 3: downscale)
    GLuint mesh_fbo, mesh_tex;
    GLuint pixel_fbo, pixel_tex;
    int mesh_w, mesh_h, pixel_w, pixel_h;
    int supersample;
    int pixel_scale;

    // Current frame state
    AmeCamera cam;
    int viewport_w, viewport_h;
    float time_sec;        // time in seconds (from SDL)
    float wind_x, wind_y;  // wind vector (pixels/sec)
    float snow_density;    // 0..1

    // Batches
    SpriteBatch* sprite_batches;
    size_t sprite_batch_count;
    size_t sprite_batch_capacity;

    MeshBatch* mesh_batches;
    size_t mesh_batch_count;
    size_t mesh_batch_capacity;

    // White fallback texture
    GLuint white_tex;
} g_pipe = {0};

// Shader compilation helpers
static GLuint compile_shader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, 1024, &n, log);
        SDL_Log("Shader compile error: %.*s", (int)n, log);
    }
    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        GLsizei n = 0;
        glGetProgramInfoLog(prog, 1024, &n, log);
        SDL_Log("Program link error: %.*s", (int)n, log);
    }
    return prog;
}

// Create white fallback texture
static void create_white_texture(void) {
    if (g_pipe.white_tex)
        return;

    glGenTextures(1, &g_pipe.white_tex);
    glBindTexture(GL_TEXTURE_2D, g_pipe.white_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    unsigned char white[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Framebuffer management
static void ensure_framebuffers(int viewport_w, int viewport_h) {
    g_pipe.supersample = 2;  // 2x supersampling for mesh pass
    g_pipe.pixel_scale = 4;  // 4x downscale for pixelation effect

    int new_mesh_w = viewport_w * g_pipe.supersample;
    int new_mesh_h = viewport_h * g_pipe.supersample;
    int new_pixel_w = viewport_w / g_pipe.pixel_scale;
    int new_pixel_h = viewport_h / g_pipe.pixel_scale;

    // Recreate mesh framebuffer if size changed
    if (g_pipe.mesh_w != new_mesh_w || g_pipe.mesh_h != new_mesh_h) {
        if (g_pipe.mesh_tex) {
            glDeleteTextures(1, &g_pipe.mesh_tex);
        }
        if (g_pipe.mesh_fbo) {
            glDeleteFramebuffers(1, &g_pipe.mesh_fbo);
        }

        glGenTextures(1, &g_pipe.mesh_tex);
        glBindTexture(GL_TEXTURE_2D, g_pipe.mesh_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_mesh_w, new_mesh_h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &g_pipe.mesh_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, g_pipe.mesh_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_pipe.mesh_tex,
                               0);
        GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        g_pipe.mesh_w = new_mesh_w;
        g_pipe.mesh_h = new_mesh_h;
    }

    // Recreate pixel framebuffer if size changed
    if (g_pipe.pixel_w != new_pixel_w || g_pipe.pixel_h != new_pixel_h) {
        if (g_pipe.pixel_tex) {
            glDeleteTextures(1, &g_pipe.pixel_tex);
        }
        if (g_pipe.pixel_fbo) {
            glDeleteFramebuffers(1, &g_pipe.pixel_fbo);
        }

        glGenTextures(1, &g_pipe.pixel_tex);
        glBindTexture(GL_TEXTURE_2D, g_pipe.pixel_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, new_pixel_w, new_pixel_h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &g_pipe.pixel_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, g_pipe.pixel_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               g_pipe.pixel_tex, 0);
        GLenum bufs2[1] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, bufs2);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        g_pipe.pixel_w = new_pixel_w;
        g_pipe.pixel_h = new_pixel_h;
    }
}

bool pipeline_init(void) {
    memset(&g_pipe, 0, sizeof(g_pipe));

    // Compile shaders
    GLuint sprite_vs = compile_shader(GL_VERTEX_SHADER, SPRITE_VS);
    GLuint sprite_fs = compile_shader(GL_FRAGMENT_SHADER, SPRITE_FS);
    g_pipe.sprite_prog = link_program(sprite_vs, sprite_fs);
    glDeleteShader(sprite_vs);
    glDeleteShader(sprite_fs);

    GLuint mesh_vs = compile_shader(GL_VERTEX_SHADER, MESH_VS);
    GLuint mesh_fs = compile_shader(GL_FRAGMENT_SHADER, MESH_FS);
    g_pipe.mesh_prog = link_program(mesh_vs, mesh_fs);
    glDeleteShader(mesh_vs);
    glDeleteShader(mesh_fs);

    GLuint comp_vs = compile_shader(GL_VERTEX_SHADER, COMP_VS);
    GLuint comp_fs = compile_shader(GL_FRAGMENT_SHADER, COMP_FS);
    g_pipe.comp_prog = link_program(comp_vs, comp_fs);
    glDeleteShader(comp_vs);
    glDeleteShader(comp_fs);

    // Snow shader program (uses COMP_VS for fullscreen triangle)
    GLuint snow_vs = compile_shader(GL_VERTEX_SHADER, COMP_VS);
    GLuint snow_fs = compile_shader(GL_FRAGMENT_SHADER, SNOW_FS);
    g_pipe.snow_prog = link_program(snow_vs, snow_fs);
    glDeleteShader(snow_vs);
    glDeleteShader(snow_fs);

    // Get uniform locations
    g_pipe.sprite_u_res = glGetUniformLocation(g_pipe.sprite_prog, "u_res");
    g_pipe.sprite_u_cam = glGetUniformLocation(g_pipe.sprite_prog, "u_cam");
    g_pipe.sprite_u_tex = glGetUniformLocation(g_pipe.sprite_prog, "u_tex");

    g_pipe.mesh_u_res = glGetUniformLocation(g_pipe.mesh_prog, "u_res");
    g_pipe.mesh_u_cam = glGetUniformLocation(g_pipe.mesh_prog, "u_cam");
    g_pipe.mesh_u_tex = glGetUniformLocation(g_pipe.mesh_prog, "u_tex");

    g_pipe.comp_u_tex = glGetUniformLocation(g_pipe.comp_prog, "u_tex");

    // Snow uniform locations
    g_pipe.snow_u_viewport = glGetUniformLocation(g_pipe.snow_prog, "u_viewport");
    g_pipe.snow_u_time = glGetUniformLocation(g_pipe.snow_prog, "u_time");
    g_pipe.snow_u_cam = glGetUniformLocation(g_pipe.snow_prog, "u_cam");
    g_pipe.snow_u_wind = glGetUniformLocation(g_pipe.snow_prog, "u_wind");
    g_pipe.snow_u_density = glGetUniformLocation(g_pipe.snow_prog, "u_density");
    g_pipe.snow_u_pixel_scale = glGetUniformLocation(g_pipe.snow_prog, "u_pixel_scale");

    // Create VAOs
    glGenVertexArrays(1, &g_pipe.sprite_vao);
    glGenBuffers(1, &g_pipe.sprite_vbo);
    glBindVertexArray(g_pipe.sprite_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_pipe.sprite_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, par));

    glGenVertexArrays(1, &g_pipe.mesh_vao);
    glGenBuffers(1, &g_pipe.mesh_vbo);
    glBindVertexArray(g_pipe.mesh_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_pipe.mesh_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, r));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)offsetof(Vtx, par));

    glGenVertexArrays(1, &g_pipe.comp_vao);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Create white texture
    create_white_texture();

    // Defaults
    g_pipe.wind_x = 5.0f;         // very slow horizontal drift
    g_pipe.wind_y = 10.0f;        // slow upward drift
    g_pipe.snow_density = 0.03f;  // very sparse - individual flakes visible

    return g_pipe.sprite_prog && g_pipe.mesh_prog && g_pipe.comp_prog && g_pipe.snow_prog;
}

void pipeline_shutdown(void) {
    // Clean up batches
    for (size_t i = 0; i < g_pipe.sprite_batch_count; i++) {
        free(g_pipe.sprite_batches[i].vertices);
    }
    free(g_pipe.sprite_batches);
    free(g_pipe.mesh_batches);

    // Clean up GL objects
    if (g_pipe.white_tex)
        glDeleteTextures(1, &g_pipe.white_tex);
    if (g_pipe.mesh_tex)
        glDeleteTextures(1, &g_pipe.mesh_tex);
    if (g_pipe.pixel_tex)
        glDeleteTextures(1, &g_pipe.pixel_tex);
    if (g_pipe.mesh_fbo)
        glDeleteFramebuffers(1, &g_pipe.mesh_fbo);
    if (g_pipe.pixel_fbo)
        glDeleteFramebuffers(1, &g_pipe.pixel_fbo);

    if (g_pipe.sprite_vbo)
        glDeleteBuffers(1, &g_pipe.sprite_vbo);
    if (g_pipe.mesh_vbo)
        glDeleteBuffers(1, &g_pipe.mesh_vbo);
    if (g_pipe.sprite_vao)
        glDeleteVertexArrays(1, &g_pipe.sprite_vao);
    if (g_pipe.mesh_vao)
        glDeleteVertexArrays(1, &g_pipe.mesh_vao);
    if (g_pipe.comp_vao)
        glDeleteVertexArrays(1, &g_pipe.comp_vao);

    if (g_pipe.sprite_prog)
        glDeleteProgram(g_pipe.sprite_prog);
    if (g_pipe.mesh_prog)
        glDeleteProgram(g_pipe.mesh_prog);
    if (g_pipe.comp_prog)
        glDeleteProgram(g_pipe.comp_prog);
    if (g_pipe.snow_prog)
        glDeleteProgram(g_pipe.snow_prog);

    memset(&g_pipe, 0, sizeof(g_pipe));
}

// Find or create sprite batch for texture
static SpriteBatch* get_sprite_batch(GLuint texture) {
    // Use white texture as fallback
    if (texture == 0)
        texture = g_pipe.white_tex;

    // Find existing batch for this texture
    for (size_t i = 0; i < g_pipe.sprite_batch_count; i++) {
        if (g_pipe.sprite_batches[i].texture == texture) {
            return &g_pipe.sprite_batches[i];
        }
    }

    // Create new batch
    if (g_pipe.sprite_batch_count >= g_pipe.sprite_batch_capacity) {
        size_t new_cap = g_pipe.sprite_batch_capacity ? g_pipe.sprite_batch_capacity * 2 : 16;
        g_pipe.sprite_batches = realloc(g_pipe.sprite_batches, new_cap * sizeof(SpriteBatch));
        g_pipe.sprite_batch_capacity = new_cap;
    }

    SpriteBatch* batch = &g_pipe.sprite_batches[g_pipe.sprite_batch_count++];
    batch->texture = texture;
    batch->vertices = NULL;
    batch->count = 0;
    batch->capacity = 0;

    return batch;
}

// Add vertices to sprite batch
static void batch_add_vertices(SpriteBatch* batch, const Vtx* verts, size_t count) {
    if (batch->count + count > batch->capacity) {
        size_t new_cap = batch->capacity ? batch->capacity * 2 : 512;
        while (new_cap < batch->count + count)
            new_cap *= 2;
        batch->vertices = realloc(batch->vertices, new_cap * sizeof(Vtx));
        batch->capacity = new_cap;
    }

    memcpy(batch->vertices + batch->count, verts, count * sizeof(Vtx));
    batch->count += count;
}

// Frame management
void pipeline_frame_begin(const AmeCamera* cam, int viewport_w, int viewport_h) {
    g_pipe.viewport_w = viewport_w;
    g_pipe.viewport_h = viewport_h;
    if (cam)
        g_pipe.cam = *cam;

    // Update time from SDL
    g_pipe.time_sec = (float)SDL_GetTicks() / 1000.0f;

    // Clear batches (reuse previously allocated batches to avoid leaks and realloc thrash)
    for (size_t i = 0; i < g_pipe.sprite_batch_count; i++) {
        g_pipe.sprite_batches[i].count = 0;
    }
    // Do NOT reset sprite_batch_count here; keep existing batches so get_sprite_batch can reuse
    // them
    g_pipe.mesh_batch_count = 0;

    // Ensure framebuffers are ready
    ensure_framebuffers(viewport_w, viewport_h);
}

void pipeline_frame_end(void) {
    // Execute multi-pass rendering:
    // Pass 1: Render meshes to offscreen texture (supersampled)
    pipeline_pass_meshes();

    // Pass 2: Composite mesh texture to pixel buffer (downscaled)
    pipeline_pass_composite();

    // Pass 3: Render sprites directly to screen (full resolution)
    pipeline_pass_sprites();

    // Pass 4: Fullscreen pixelated snow overlay
    pipeline_pass_snow();
}

// Sprite submission (batched by texture)
void pipeline_sprite_quad(float cx,
                          float cy,
                          float w,
                          float h,
                          unsigned int texture,
                          float r,
                          float g,
                          float b,
                          float a) {
    // Fallback to rotation with 0 angle to unify code path
    pipeline_sprite_quad_rot(cx, cy, w, h, 0.0f, texture, r, g, b, a);
}

static void rotate_point(float ox, float oy, float angle, float* io_x, float* io_y) {
    float s = sinf(angle), c = cosf(angle);
    float x = *io_x - ox;
    float y = *io_y - oy;
    float rx = x * c - y * s;
    float ry = x * s + y * c;
    *io_x = rx + ox;
    *io_y = ry + oy;
}

// Rotated sprite submission (angle in radians)
void pipeline_sprite_quad_rot(float cx,
                              float cy,
                              float w,
                              float h,
                              float radians,
                              unsigned int texture,
                              float r,
                              float g,
                              float b,
                              float a) {
    SpriteBatch* batch = get_sprite_batch(texture);

    float x0 = cx - w * 0.5f;
    float y0 = cy - h * 0.5f;
    float x1 = cx + w * 0.5f;
    float y1 = cy + h * 0.5f;

    Vtx quad[6] = {{x0, y0, 0, 0, r, g, b, a, 1.0f}, {x1, y0, 1, 0, r, g, b, a, 1.0f},
                   {x0, y1, 0, 1, r, g, b, a, 1.0f}, {x1, y0, 1, 0, r, g, b, a, 1.0f},
                   {x1, y1, 1, 1, r, g, b, a, 1.0f}, {x0, y1, 0, 1, r, g, b, a, 1.0f}};
    // Rotate all verts around center (cx, cy)
    for (int i = 0; i < 6; i++) {
        rotate_point(cx, cy, radians, &quad[i].x, &quad[i].y);
    }
    batch_add_vertices(batch, quad, 6);
}

// Mesh submission (for offscreen rendering)
void pipeline_mesh_submit(const AmeLocalMesh* mesh,
                          float tx,
                          float ty,
                          float tz,
                          float sx,
                          float sy,
                          float sz,
                          float r,
                          float g,
                          float b,
                          float a) {
    if (!mesh || mesh->count == 0 || !mesh->pos)
        return;

    // For triangle-level sorting, we don't need to calculate mesh depth here
    // Instead, we'll sort individual triangles during rendering

    // Expand mesh batches if needed
    if (g_pipe.mesh_batch_count >= g_pipe.mesh_batch_capacity) {
        size_t new_cap = g_pipe.mesh_batch_capacity ? g_pipe.mesh_batch_capacity * 2 : 16;
        g_pipe.mesh_batches = realloc(g_pipe.mesh_batches, new_cap * sizeof(MeshBatch));
        g_pipe.mesh_batch_capacity = new_cap;
    }

    MeshBatch* batch = &g_pipe.mesh_batches[g_pipe.mesh_batch_count++];
    batch->mesh = mesh;
    batch->tx = tx;
    batch->ty = ty;
    batch->tz = tz;
    batch->sx = sx;
    batch->sy = sy;
    batch->sz = sz;
    batch->r = r;
    batch->g = g;
    batch->b = b;
    batch->a = a;
    batch->depth = 0.0f;  // Not used for triangle-level sorting
}

// Pass 1: Render sprites to screen (full resolution)
void pipeline_pass_sprites(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pipe.viewport_w, g_pipe.viewport_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_pipe.sprite_prog);
    glBindVertexArray(g_pipe.sprite_vao);

    if (g_pipe.sprite_u_res >= 0) {
        glUniform2f(g_pipe.sprite_u_res, (float)g_pipe.viewport_w, (float)g_pipe.viewport_h);
    }

    // Use exact camera position for smoother motion at high speed
    if (g_pipe.sprite_u_cam >= 0) {
        glUniform4f(g_pipe.sprite_u_cam, g_pipe.cam.x, g_pipe.cam.y, g_pipe.cam.zoom,
                    g_pipe.cam.rotation);
    }

    if (g_pipe.sprite_u_tex >= 0) {
        glUniform1i(g_pipe.sprite_u_tex, 0);
    }

    // Render each sprite batch
    for (size_t i = 0; i < g_pipe.sprite_batch_count; i++) {
        SpriteBatch* batch = &g_pipe.sprite_batches[i];
        if (batch->count == 0)
            continue;

        // Upload vertices for this batch
        glBindBuffer(GL_ARRAY_BUFFER, g_pipe.sprite_vbo);
        glBufferData(GL_ARRAY_BUFFER, batch->count * sizeof(Vtx), batch->vertices, GL_DYNAMIC_DRAW);

        // Bind texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch->texture);

        // Draw
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)batch->count);
    }

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// Comparison function for triangle depth sorting (smaller Z values render behind - ascending order)
static int compare_triangle_depth(const void* a, const void* b) {
    const Triangle* tri_a = (const Triangle*)a;
    const Triangle* tri_b = (const Triangle*)b;
    if (tri_a->depth < tri_b->depth)
        return -1;  // a comes first (smaller Z behind)
    if (tri_a->depth > tri_b->depth)
        return 1;  // b comes first (larger Z in front)
    return 0;      // equal depth
}

// Pass 2: Render meshes to offscreen texture (supersampled)
void pipeline_pass_meshes(void) {
    if (g_pipe.mesh_batch_count == 0) {
        // Clear mesh texture if no meshes to render
        glBindFramebuffer(GL_FRAMEBUFFER, g_pipe.mesh_fbo);
        glViewport(0, 0, g_pipe.mesh_w, g_pipe.mesh_h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, g_pipe.mesh_fbo);
    glViewport(0, 0, g_pipe.mesh_w, g_pipe.mesh_h);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_BLEND);

    glUseProgram(g_pipe.mesh_prog);
    glBindVertexArray(g_pipe.mesh_vao);

    // Use supersampled resolution for mesh rendering
    if (g_pipe.mesh_u_res >= 0) {
        glUniform2f(g_pipe.mesh_u_res, (float)g_pipe.mesh_w, (float)g_pipe.mesh_h);
    }

    // Use exact camera position for smoother motion at high speed
    if (g_pipe.mesh_u_cam >= 0) {
        glUniform4f(g_pipe.mesh_u_cam, g_pipe.cam.x, g_pipe.cam.y,
                    g_pipe.cam.zoom * g_pipe.supersample, g_pipe.cam.rotation);
    }

    if (g_pipe.mesh_u_tex >= 0) {
        glUniform1i(g_pipe.mesh_u_tex, 0);
    }

    // Collect all triangles from all meshes and sort them by depth
    size_t total_triangles = 0;
    for (size_t i = 0; i < g_pipe.mesh_batch_count; i++) {
        total_triangles += g_pipe.mesh_batches[i].mesh->count / 3;
    }

    if (total_triangles == 0) {
        return;
    }

    Triangle* triangles = malloc(total_triangles * sizeof(Triangle));
    size_t tri_index = 0;

    // Build triangles with depth information
    for (size_t i = 0; i < g_pipe.mesh_batch_count; i++) {
        const MeshBatch* batch = &g_pipe.mesh_batches[i];
        const AmeLocalMesh* mesh = batch->mesh;

        // Process triangles (groups of 3 vertices)
        for (size_t v = 0; v < mesh->count; v += 3) {
            if (v + 2 >= mesh->count)
                break;  // Ensure we have a complete triangle

            Triangle* tri = &triangles[tri_index++];
            float total_z = 0.0f;

            // Process 3 vertices of the triangle
            for (int j = 0; j < 3; j++) {
                size_t vert_idx = v + j;

                // Extract xyz from mesh, transform, and use xy for rendering
                float vx = mesh->pos[vert_idx * 3 + 0];  // vertex x
                float vy = mesh->pos[vert_idx * 3 + 1];  // vertex y
                float vz = mesh->pos[vert_idx * 3 + 2];  // vertex z

                // Apply object transformation
                float px = vx * batch->sx + batch->tx;
                float py = vy * batch->sy + batch->ty;
                float pz = vz * batch->sz + batch->tz;  // transformed Z for depth & parallax

                total_z += pz;

                float u = mesh->uv ? mesh->uv[vert_idx * 2 + 0] : 0.0f;
                float uv = mesh->uv ? mesh->uv[vert_idx * 2 + 1] : 0.0f;

                // Map depth to parallax factor using inverse falloff: a_par = 1 / (1 + K*|Z|)
                // Large |Z| (far) -> near zero movement; small |Z| (near) -> ~1.0
                float par = 1.0f / (1.0f + fabsf(pz) * PARALLAX_K);
                // Optional cap to avoid overscaling; keep within [0, 1]
                if (par < 0.0f)
                    par = 0.0f;
                if (par > 1.0f)
                    par = 1.0f;

                tri->verts[j] = (Vtx){px, py, u, uv, batch->r, batch->g, batch->b, batch->a, par};
            }

            // Calculate average depth for this triangle
            tri->depth = total_z / 3.0f;
        }
    }

    // Sort triangles by depth (higher depth renders behind)
    if (total_triangles > 1) {
        qsort(triangles, total_triangles, sizeof(Triangle), compare_triangle_depth);
    }

    // Render all sorted triangles
    size_t total_vertices = total_triangles * 3;
    Vtx* all_verts = malloc(total_vertices * sizeof(Vtx));

    for (size_t i = 0; i < total_triangles; i++) {
        memcpy(&all_verts[i * 3], triangles[i].verts, 3 * sizeof(Vtx));
    }

    // Upload and render all triangles at once
    glBindBuffer(GL_ARRAY_BUFFER, g_pipe.mesh_vbo);
    glBufferData(GL_ARRAY_BUFFER, total_vertices * sizeof(Vtx), all_verts, GL_DYNAMIC_DRAW);

    glActiveTexture(GL_TEXTURE0);
    // Use the texture from the first mesh (assuming all share the same texture)
    GLuint tex = (g_pipe.mesh_batch_count > 0 && g_pipe.mesh_batches[0].mesh->texture)
                     ? g_pipe.mesh_batches[0].mesh->texture
                     : g_pipe.white_tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)total_vertices);

    free(triangles);
    free(all_verts);

    // Generate mipmaps for better downsampling
    glBindTexture(GL_TEXTURE_2D, g_pipe.mesh_tex);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
}

// Pass 3: Composite offscreen texture to screen (downscaled)
void pipeline_pass_composite(void) {
    // First, downsample mesh texture to pixel buffer
    glBindFramebuffer(GL_FRAMEBUFFER, g_pipe.pixel_fbo);
    glViewport(0, 0, g_pipe.pixel_w, g_pipe.pixel_h);

    glUseProgram(g_pipe.comp_prog);
    glBindVertexArray(g_pipe.comp_vao);

    if (g_pipe.comp_u_tex >= 0) {
        glUniform1i(g_pipe.comp_u_tex, 0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_pipe.mesh_tex);

    // Use linear filtering for nice downsampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Now composite pixel buffer to screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pipe.viewport_w, g_pipe.viewport_h);

    // Clear screen
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);  // Sky blue background
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_pipe.pixel_tex);

    // Use nearest filtering for crisp pixel look
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// Pass 4: Fullscreen pixelated snow overlay
void pipeline_pass_snow(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_pipe.viewport_w, g_pipe.viewport_h);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_pipe.snow_prog);
    glBindVertexArray(g_pipe.comp_vao);

    if (g_pipe.snow_u_viewport >= 0)
        glUniform2f(g_pipe.snow_u_viewport, (float)g_pipe.viewport_w, (float)g_pipe.viewport_h);
    if (g_pipe.snow_u_time >= 0)
        glUniform1f(g_pipe.snow_u_time, g_pipe.time_sec);
    if (g_pipe.snow_u_cam >= 0)
        glUniform2f(g_pipe.snow_u_cam, g_pipe.cam.x, g_pipe.cam.y);
    if (g_pipe.snow_u_wind >= 0)
        glUniform2f(g_pipe.snow_u_wind, g_pipe.wind_x, g_pipe.wind_y);
    if (g_pipe.snow_u_density >= 0)
        glUniform1f(g_pipe.snow_u_density, g_pipe.snow_density);
    if (g_pipe.snow_u_pixel_scale >= 0)
        glUniform1f(g_pipe.snow_u_pixel_scale, (float)g_pipe.pixel_scale);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

// Legacy compatibility functions
void pipeline_begin(const AmeCamera* cam, int viewport_w, int viewport_h) {
    pipeline_frame_begin(cam, viewport_w, viewport_h);
}

void pipeline_end(void) {
    pipeline_frame_end();
}
