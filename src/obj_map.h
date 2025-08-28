#pragma once
#include <stdbool.h>
#include "render/pipeline.h"
#ifdef __cplusplus
extern "C" {
#endif

// Load OBJ and create static colliders inferred from object names:
// - BoxCollider*, CircleCollider*, EdgeCollider*, ChainCollider*, MeshCollider*
// Visual geometry (non-collider) is packed into out_mesh for rendering (optional).
// UVs are taken from OBJ texcoords; texture is loaded from the material's diffuse map (map_Kd).
bool load_obj_map(const char* path, AmeLocalMesh* out_mesh);
void free_obj_map(AmeLocalMesh* mesh);

#ifdef __cplusplus
}
#endif
