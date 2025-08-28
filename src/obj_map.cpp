#include "obj_map.h"
#include <cstring>
#include <string>
#include <vector>
#include "physics.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

static bool has_prefix(const std::string& s, const char* p) {
    return s.rfind(p, 0) == 0;
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
    std::vector<float> vis;
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
        // Visual contribution
        for (const auto& idx : sh.mesh.indices) {
            size_t vi = size_t(idx.vertex_index) * 3;
            float x = attrib.vertices[vi + 0];
            float y = attrib.vertices[vi + 1];
            vis.push_back(x);
            vis.push_back(y);
        }
    }
    if (out_mesh && !vis.empty()) {
        float* pos = (float*)malloc(vis.size() * sizeof(float));
        memcpy(pos, vis.data(), vis.size() * sizeof(float));
        out_mesh->pos = pos;
        out_mesh->count = (unsigned int)(vis.size() / 2);
    }
    return true;
}

void free_obj_map(AmeLocalMesh* mesh) {
    if (!mesh)
        return;
    free(mesh->pos);
    mesh->pos = nullptr;
    mesh->count = 0;
}
