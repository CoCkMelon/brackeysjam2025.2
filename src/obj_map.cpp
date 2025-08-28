#include "obj_map.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <glad/gl.h>
#include <cstring>
#include <string>
#include <vector>
#include "physics.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static bool has_prefix(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
        // Collect vertices referenced by this shape in order (use OBJ x and y for 2D)
        std::vector<float> xs, ys;
        xs.reserve(sh.mesh.indices.size());
        ys.reserve(sh.mesh.indices.size());
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (const auto& idx : sh.mesh.indices) {
            size_t vi = size_t(idx.vertex_index) * 3;
            float x = attrib.vertices[vi + 0];
            float y = attrib.vertices[vi + 1];
            xs.push_back(x);
            ys.push_back(y);
            if (x < minx)
                minx = x;
            if (x > maxx)
                maxx = x;
            if (y < miny)
                miny = y;
            if (y > maxy)
                maxy = y;
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
        // Visual contribution with UVs
        for (const auto& idx : sh.mesh.indices) {
            size_t vi = size_t(idx.vertex_index) * 3;
            float x = attrib.vertices[vi + 0];
            float y = attrib.vertices[vi + 1];
            vis.push_back(x);
            vis.push_back(y);
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
        out_mesh->count = (unsigned int)(vis.size() / 2);
        if (uvs.size() == vis.size()) {
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
