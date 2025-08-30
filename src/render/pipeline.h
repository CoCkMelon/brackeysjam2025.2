#pragma once
#include <glad/gl.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

// Forward declare camera from engine C API
typedef struct AmeCamera AmeCamera;

// Minimal mesh struct (positions are 3D x,y,z with optional UV)
typedef struct {
    float* pos;            // interleaved x,y,z triplets
    float* uv;             // interleaved u,v pairs (can be NULL)
    unsigned int count;    // number of vertices (not floats)
    unsigned int texture;  // GL texture id (0 if none)
} AmeLocalMesh;

// 3-Pass rendering pipeline:
// Pass 1: Sprites (batched by texture, full resolution)
// Pass 2: Meshes (rendered to offscreen texture, supersampled)
// Pass 3: Composite (downscale mesh texture to screen)
// Pass 4: Snow overlay (fullscreen pixelated snow)

bool pipeline_init(void);
void pipeline_shutdown(void);

// Frame management - call these to setup the 3-pass rendering
void pipeline_frame_begin(const AmeCamera* cam, int viewport_w, int viewport_h);
void pipeline_frame_end(void);

// Pass 1: Sprite submission (batched by texture)
void pipeline_sprite_quad(float cx,
                          float cy,
                          float w,
                          float h,
                          unsigned int texture,
                          float r,
                          float g,
                          float b,
                          float a);
// Rotated sprite submission (angle in radians, rotates around center)
void pipeline_sprite_quad_rot(float cx,
                              float cy,
                              float w,
                              float h,
                              float radians,
                              unsigned int texture,
                              float r,
                              float g,
                              float b,
                              float a);

// Pass 2: Mesh submission (rendered to offscreen target)
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
                          float a);

// Internal pass management (automatically called by frame_begin/end)
void pipeline_pass_sprites(void);    // Render batched sprites to screen
void pipeline_pass_meshes(void);     // Render meshes to offscreen texture
void pipeline_pass_composite(void);  // Composite offscreen texture to screen
void pipeline_pass_snow(void);       // Fullscreen pixelated snow overlay

// Legacy compatibility (deprecated - use frame_begin/end instead)
void pipeline_begin(const AmeCamera* cam, int viewport_w, int viewport_h);
void pipeline_end(void);

#ifdef __cplusplus
}
#endif
