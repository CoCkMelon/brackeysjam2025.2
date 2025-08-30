/*
Object must be a mesh, even if empty.
Recognized object name keywords in OBJ shapes:

Prefixes:
- "Trigger <Name>"      -> Creates a trigger AABB named <Name> (fires repeatedly while overlapped)
- "TriggerGrenade"      -> Legacy trigger name (kept for back-compat)
- "TriggerRocket"       -> Legacy trigger name (kept for back-compat)
- "TriggerTurretShot"   -> Legacy trigger name (kept for back-compat)
- "Mine"                -> Spawns a mine at the shape's center
- "Turret"              -> Spawns a turret at the shape's center
- "Fuel[Amount]"        -> Spawns a fuel pickup; e.g. Fuel50 gives 50 (default 25)
- "Spawn" or "SpawnPoint" -> Adds a player spawn point at the shape's center
- "BoxCollider"         -> Static box collider from the shape's AABB
- "CircleCollider"      -> Static circle collider from the shape's AABB radius
- "EdgeCollider"        -> Static edge from the first two vertices
- "ChainCollider[Loop|Closed]" -> Static chain from all vertices; closed if name contains
Loop/Closed
- "MeshCollider"        -> Static triangle mesh collider from the shape's triangles

Tags (substring anywhere in name):
- "Saw"                 -> Spawns a saw (radius from shape size). Visual mesh not used.
- "Spike"               -> Marks triangles as spike colliders; still contributes to visual mesh.
*/
#include "obj_map.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/gl.h>
#include <cstring>
#include <string>
#include <vector>
#include "gameplay.h"
#include "physics.h"
#include "triggers.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static bool has_prefix(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
}
static bool has_tag(const std::string& s, const char* t) {
    return s.find(t) != std::string::npos;
}

static std::string dirname_from_path(const char* path) {
    if (!path)
        return std::string();
    const char* last_slash = strrchr(path, '/');
    if (!last_slash)
        return std::string();
    return std::string(path, last_slash - path + 1);  // include trailing slash
}

static GLuint load_texture_absolute(const char* filename) {
    SDL_Surface* surf = IMG_Load(filename);
    if (!surf)
        return 0;
    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!conv)
        return 0;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 conv->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SDL_DestroySurface(conv);
    return tex;
}

bool load_obj_map(const char* path, AmeLocalMesh* out_mesh) {
    if (out_mesh) {
        out_mesh->pos = nullptr;
        out_mesh->uv = nullptr;
        out_mesh->count = 0;
        out_mesh->texture = 0;
    }
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig cfg;
    cfg.triangulate = true;
    if (!reader.ParseFromFile(path, cfg))
        return false;

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& materials = reader.GetMaterials();

    std::vector<float> vis;
    std::vector<float> uvs;
    vis.reserve(1024);
    uvs.reserve(1024);

    for (const auto& sh : shapes) {
        std::string name = sh.name;
        // Collect vertices referenced by this shape in order (use OBJ x, y, and z)
        std::vector<float> xs, ys, zs;
        xs.reserve(sh.mesh.indices.size());
        ys.reserve(sh.mesh.indices.size());
        zs.reserve(sh.mesh.indices.size());
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (const auto& idx : sh.mesh.indices) {
            size_t vi = size_t(idx.vertex_index) * 3;
            float x = attrib.vertices[vi + 0];
            float y = attrib.vertices[vi + 1];
            float z = attrib.vertices[vi + 2];
            xs.push_back(x);
            ys.push_back(y);
            zs.push_back(z);
            if (x < minx)
                minx = x;
            if (x > maxx)
                maxx = x;
            if (y < miny)
                miny = y;
            if (y > maxy)
                maxy = y;
        }
        // Generic trigger support: objects named "Trigger <name>" create a trigger with that name
        if (has_prefix(name, "Trigger ")) {
            // Create a triggers AABB for this shape
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            float w = (maxx - minx);
            float h = (maxy - miny);
            Aabb box = {cx, cy, w, h};
            // Extract trigger name after the space
            std::string trig_name = name.substr(8);  // len("Trigger ") == 8
            // Allocate payload with center to pass to callback
            GameplayTriggerUser* u = (GameplayTriggerUser*)malloc(sizeof(GameplayTriggerUser));
            if (u) {
                u->x = cx;
                u->y = cy;
            }
            // Not once: can keep firing when overlapped
            // Duplicate name to keep it valid beyond this function
            char* trig_copy = SDL_strdup(trig_name.c_str());
            triggers_add(trig_copy ? trig_copy : trig_name.c_str(), box, 1, gameplay_on_trigger,
                         (void*)u);
            continue;
        }
        // Back-compat: support legacy concatenated trigger names (incl. TriggerDialogue)
        if (has_prefix(name, "TriggerGrenade") || has_prefix(name, "TriggerRocket") ||
            has_prefix(name, "TriggerTurretShot") || has_prefix(name, "TriggerDialogue")) {
            // Create a triggers AABB for this shape
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            float w = (maxx - minx);
            float h = (maxy - miny);
            Aabb box = {cx, cy, w, h};
            // Allocate payload with center to pass to callback
            GameplayTriggerUser* u = (GameplayTriggerUser*)malloc(sizeof(GameplayTriggerUser));
            if (u) {
                u->x = cx;
                u->y = cy;
            }
            // Not once: can keep firing when overlapped
            // Duplicate name to keep it valid beyond this function
            char* name_copy = SDL_strdup(name.c_str());
            triggers_add(name_copy ? name_copy : name.c_str(), box, 1, gameplay_on_trigger,
                         (void*)u);
            continue;
        }
        if (has_prefix(name, "Mine")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            spawn_mine(cx, cy);
            continue;
        }
        if (has_prefix(name, "Turret")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            spawn_turret(cx, cy);
            continue;
        }
        if (has_prefix(name, "Fuel")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            // Try to parse amount after prefix, e.g., Fuel50
            float amount = 25.0f;
            const char* p = name.c_str() + 4;  // after "Fuel"
            if (*p) {
                int val = atoi(p);
                if (val > 0)
                    amount = (float)val;
            }
            spawn_fuel_pickup(cx, cy, amount);
            continue;
        }
        if (has_prefix(name, "Spawn") || has_prefix(name, "SpawnPoint")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            gameplay_add_spawn_point(cx, cy);
            continue;
        }
        if (has_tag(name, "Saw") || has_tag(name, "Spike")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            float w = (maxx - minx);
            float h = (maxy - miny);
            if (has_tag(name, "Spike")) {
                // Create spike collider from this shape's triangles with spike flag; keep visual
                // mesh
                if (xs.size() >= 3) {
                    std::vector<float> tri;
                    tri.reserve(xs.size() * 2);
                    for (size_t i = 0; i < xs.size(); ++i) {
                        tri.push_back(xs[i]);
                        tri.push_back(ys[i]);
                    }
                    physics_create_static_mesh_triangles_tagged(tri.data(), (int)xs.size(), 0.8f,
                                                                PHYS_FLAG_SPIKE);
                }
                // do not continue; allow visual geometry accumulation
            } else {
                float r = 0.5f * fminf(w, h);
                gameplay_spawn_saw(cx, cy, r > 2.0f ? r : 6.0f);
                continue;  // saw uses sprite, not visual mesh
            }
        }
        if (has_prefix(name, "BoxCollider")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            float w = (maxx - minx);
            float h = (maxy - miny);
            if (w > 0 && h > 0)
                physics_create_static_box(cx, cy, w, h, 0.8f);
            continue;
        }
        if (has_prefix(name, "CircleCollider")) {
            float cx = 0.5f * (minx + maxx);
            float cy = 0.5f * (miny + maxy);
            float r = 0.5f * ((maxx - minx + maxy - miny) * 0.5f);
            physics_create_static_circle(cx, cy, r, 0.8f);
            continue;
        }
        if (has_prefix(name, "EdgeCollider")) {
            if (xs.size() >= 2) {
                physics_create_static_edge(xs[0], ys[0], xs[1], ys[1], 0.8f);
            }
            continue;
        }
        if (has_prefix(name, "ChainCollider")) {
            if (xs.size() >= 2) {
                std::vector<float> pairs;
                pairs.reserve(xs.size() * 2);
                for (size_t i = 0; i < xs.size(); ++i) {
                    pairs.push_back(xs[i]);
                    pairs.push_back(ys[i]);
                }
                bool loop = (name.find("Loop") != std::string::npos) ||
                            (name.find("Closed") != std::string::npos);
                physics_create_static_chain(pairs.data(), (int)xs.size(), loop, 0.8f);
            }
            continue;
        }
        if (has_prefix(name, "MeshCollider")) {
            // Use triangles directly
            if (xs.size() >= 3) {
                std::vector<float> tri;
                tri.reserve(xs.size() * 2);
                for (size_t i = 0; i < xs.size(); ++i) {
                    tri.push_back(xs[i]);
                    tri.push_back(ys[i]);
                }
                physics_create_static_mesh_triangles(tri.data(), (int)xs.size(), 0.8f);
            }
            continue;
        }
        // Visual contribution with UVs (store xyz coordinates)
        for (const auto& idx : sh.mesh.indices) {
            size_t vi = size_t(idx.vertex_index) * 3;
            float x = attrib.vertices[vi + 0];
            float y = attrib.vertices[vi + 1];
            float z = attrib.vertices[vi + 2];
            vis.push_back(x);
            vis.push_back(y);
            vis.push_back(z);
            if (idx.texcoord_index >= 0) {
                size_t ti = size_t(idx.texcoord_index) * 2;
                float u = attrib.texcoords.size() > ti + 1 ? attrib.texcoords[ti + 0] : 0.0f;
                float v = attrib.texcoords.size() > ti + 1 ? attrib.texcoords[ti + 1] : 0.0f;
                uvs.push_back(u);
                uvs.push_back(v);
            } else {
                uvs.push_back(0.0f);
                uvs.push_back(0.0f);
            }
        }
    }

    // Load texture from first material that has a diffuse texture (map_Kd)
    GLuint map_tex = 0;
    if (!materials.empty()) {
        // Determine OBJ directory for relative paths
        std::string objdir = dirname_from_path(path);
        for (const auto& m : materials) {
            if (!m.diffuse_texname.empty()) {
                const std::string& tn = m.diffuse_texname;
                std::string texpath;
                if (!tn.empty() && tn[0] == '/') {
                    texpath = tn;  // absolute
                } else if (!objdir.empty()) {
                    texpath = objdir + tn;  // relative to OBJ dir
                } else {
                    texpath = tn;  // as-is
                }
                map_tex = load_texture_absolute(texpath.c_str());
                if (map_tex != 0)
                    break;
                // as a last resort, try the bare name (cwd)
                map_tex = load_texture_absolute(tn.c_str());
                if (map_tex != 0)
                    break;
                SDL_Log("OBJ map: failed to load diffuse texture: %s", texpath.c_str());
            }
        }
    }

    if (out_mesh && !vis.empty()) {
        float* pos = (float*)malloc(vis.size() * sizeof(float));
        memcpy(pos, vis.data(), vis.size() * sizeof(float));
        out_mesh->pos = pos;
        out_mesh->count = (unsigned int)(vis.size() / 3);  // xyz triplets
        if (uvs.size() ==
            vis.size() / 3 * 2) {  // uvs are still pairs, so vis.size()/3*2 pairs expected
            float* uv = (float*)malloc(uvs.size() * sizeof(float));
            memcpy(uv, uvs.data(), uvs.size() * sizeof(float));
            out_mesh->uv = uv;
        }
        out_mesh->texture = map_tex;  // 0 if none loaded; pipeline will fallback to white
    }
    return true;
}

void free_obj_map(AmeLocalMesh* mesh) {
    if (!mesh)
        return;
    free(mesh->pos);
    free(mesh->uv);
    if (mesh->texture) {
        glDeleteTextures(1, &mesh->texture);
    }
    mesh->pos = nullptr;
    mesh->uv = nullptr;
    mesh->texture = 0;
    mesh->count = 0;
}
