#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "render/pipeline.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    AmeLocalMesh mesh;
} ObjScene;

// Load a minimal OBJ (vertices 'v' and faces 'f').
// Uses x and z from OBJ as x and y in 2D (Y-up), ignores normals/UVs/materials.
// Returns true on success.
bool obj_load_2d(const char* path, ObjScene* out);
void obj_free(ObjScene* s);

#ifdef __cplusplus
}
#endif
