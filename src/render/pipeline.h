#pragma once
#include <glad/gl.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

// Forward declare camera from engine C API
typedef struct AmeCamera AmeCamera;

// Minimal mesh struct (positions are 2D x,y with optional UV)
typedef struct {
  float *pos;   // interleaved x,y pairs
  float *uv;    // interleaved u,v pairs (can be NULL)
  unsigned int count; // number of vertices (not floats)
  unsigned int texture; // GL texture id (0 if none)
} AmeLocalMesh;

bool pipeline_init(void);
void pipeline_shutdown(void);

// Begin a frame with camera (Y-up bottom-left center coords)
void pipeline_begin(const AmeCamera *cam, int viewport_w, int viewport_h);
// Submit a sprite quad (x,y at center, Y-up), size in pixels or world units depending on your art
void pipeline_sprite_quad(float cx, float cy, float w, float h, unsigned int texture, float r, float g, float b, float a);
// Submit a raw mesh
void pipeline_mesh_submit(const AmeLocalMesh *mesh, float tx, float ty, float sx, float sy, float r, float g, float b, float a);
// End and flush
void pipeline_end(void);

#ifdef __cplusplus
}
#endif

