#pragma once
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "unitylike/Scene.h"
#include "CarController.h"
#include "CarCameraController.h"
#include <glad/gl.h>
#include <box2d/box2d.h>
extern "C" {
#include "ame/physics.h"
}
#include "globals.h"
#include "ame/obj.h"
#include "ame/render_pipeline_ecs.h"
#include "ame/ecs.h"
#include "ame/collider2d_extras.h"
#include "ame/collider2d_system.h"

using namespace unitylike;

// GameManager builds the car scene: physics, ground, car, and camera
class CarGameManager : public MongooseBehaviour {
public:
    int screenWidth = 1280;
    int screenHeight = 720;
    float gravityY = -30.0f; // tuned for car scale

private:
    GameObject car;
    GameObject cameraObj;
    GameObject groundObj;
    // We avoid storing created GameObjects to minimize ECS copying during creation
    AmePhysicsWorld* physics = nullptr;
    CarCameraController* cameraCtl = nullptr;

    // Atlas data
    GLuint atlasTex = 0;
    int atlasW = 0, atlasH = 0;
    glm::vec4 uvWheel{0,0,1,1};
    glm::vec4 uvNoise{0,0,1,1};
    glm::vec4 uvSolid{0,0,1,1};

    int obstaclesTotal = 6;
    int obstaclesSpawned = 0;

    // Build a combined texture atlas: circle (wheel), noise (obstacles), solid (1x1 white)
    void build_atlas() {
        // Layout: 64x32
        atlasW = 64; atlasH = 32;
        const int circleX=0, circleY=0, circleW=32, circleH=32;
        const int noiseX=32, noiseY=0, noiseW=16, noiseH=16;
        const int solidX=48, solidY=0, solidW=1, solidH=1;
        std::vector<unsigned> pix(atlasW * atlasH, 0x00000000u);

        // Circle: white opaque circle on transparent background
        for (int y=0; y<circleH; ++y) {
            for (int x=0; x<circleW; ++x) {
                float cx = (x + 0.5f) - circleW * 0.5f;
                float cy = (y + 0.5f) - circleH * 0.5f;
                float r = circleW * 0.5f - 1.0f;
                float d = sqrtf(cx*cx + cy*cy);
                unsigned a = (d <= r) ? 0xFFu : 0x00u;
                unsigned color = (a << 24) | 0x00FFFFFFu; // white with alpha
                pix[(circleY + y) * atlasW + (circleX + x)] = color;
            }
        }
        // Noise: grayscale with slight blue tint
        uint32_t seed = 1337u;
        auto rnd = [&](){ seed = seed*1664525u + 1013904223u; return seed; };
        for (int y=0; y<noiseH; ++y) {
            for (int x=0; x<noiseW; ++x) {
                uint8_t v = (uint8_t)(rnd() >> 24);
                unsigned color = (0xFFu<<24) | (v<<16) | (v<<8) | (unsigned)std::min<int>(255, v+20);
                pix[(noiseY + y) * atlasW + (noiseX + x)] = color;
            }
        }
        // Solid: 1x1 white
        pix[(solidY + 0) * atlasW + (solidX + 0)] = 0xFFFFFFFFu;

        // Upload to GL
        if (atlasTex == 0) glGenTextures(1, &atlasTex);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasW, atlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());

        // Compute UVs for renderer, which expects v0 as top and v1 as bottom
        auto uv = [&](int x,int y,int w,int h){
            float u0 = (float)x / (float)atlasW;
            float u1 = (float)(x + w) / (float)atlasW;
            float vb = (float)y / (float)atlasH;           // bottom
            float vt = (float)(y + h) / (float)atlasH;     // top
            return glm::vec4(u0, vt, u1, vb); // v0=top, v1=bottom
        };
        uvWheel = uv(circleX, circleY, circleW, circleH);
        uvNoise = uv(noiseX, noiseY, noiseW, noiseH);
        uvSolid = uv(solidX, solidY, solidW, solidH);
    }

public:
    void Awake() {
        physics = ame_physics_world_create(0.0f, gravityY, fixedTimeStep);
        // Register collider systems and physics components so lookups work
        if (world) {
            ame_collider2d_extras_register(world);
            ame_collider2d_system_register(world);
        }
        if (ameWorld) {
            (void)ame_physics_register_body_component(ameWorld);
            (void)ame_physics_register_transform_component(ameWorld);
        }
    }

    void Start() {
        // Build atlas first so visuals can reference it
        build_atlas();

        b2World* w = physics ? (b2World*)physics->world : nullptr;

        // Ground physics (static) and visual
        if (w) {
            // Physics body
            b2BodyDef gd; gd.type = b2_staticBody; gd.position.Set(0.0f, 0.0f);
            b2Body* ground = w->CreateBody(&gd);
            b2PolygonShape gshape;
            float groundY = 0.0f;
            gshape.SetAsBox(100.0f, 0.5f, b2Vec2(0.0f, groundY - 0.5f), 0.0f);
            b2FixtureDef gfd; gfd.shape = &gshape; gfd.friction = 0.9f;
            ground->CreateFixture(&gfd);

            // Visual GameObject
            groundObj = gameObject().scene()->Create("Ground");
            auto& gr = groundObj.AddComponent<SpriteRenderer>();
            gr.texture(atlasTex);
            // Use 1x1 solid texel and tint to desired ground color
            gr.uv(uvSolid.x, uvSolid.y, uvSolid.z, uvSolid.w);
            gr.color({0.15f, 0.18f, 0.20f, 1.0f});
            gr.size({200.0f, 1.0f});
            gr.sortingLayer(0); gr.orderInLayer(0); gr.z(0.0f);
            // Centered at (0, groundY-0.5)
            groundObj.transform().position({0.0f, groundY - 0.5f, 0.0f});

            // Import an OBJ file (positions in 2D, uv optional)
            AmeObjImportConfig cfg = {0};
            cfg.create_colliders = 1; // allow name-prefixed colliders if present in the file
            const char* obj_path = "assets/car_village.obj";
            AmeObjImportResult r = ame_obj_import_obj(world, obj_path, &cfg);
            SDL_Log("OBJ import: root=%llu objects=%d meshes=%d colliders=%d", (unsigned long long)r.root, r.objects_created, r.meshes_created, r.colliders_created);

            // TODO: Add proper cleanup observers for mesh data leaks

            // Load textures referenced by .mtl into Material.tex (no Sprite needed)
            struct MaterialData { uint32_t tex; float r,g,b,a; int dirty; };
            struct MaterialTexPath { const char* path; };
            auto load_texture_rgba8 = [](const char* path) -> GLuint {
                if (!path || !*path) return 0;
                SDL_Surface* surf = IMG_Load(path);
                if (!surf) { SDL_Log("IMG_Load failed for %s: %s", path, SDL_GetError()); return 0; }
                SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
                SDL_DestroySurface(surf);
                if (!conv) { SDL_Log("ConvertSurface to RGBA32 failed for %s: %s", path, SDL_GetError()); return 0; }
                GLuint tex = 0; glGenTextures(1, &tex);
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                SDL_DestroySurface(conv);
                return tex;
            };
            std::unordered_map<std::string, GLuint> tex_cache;
            ecs_entity_t mat_id = ecs_lookup(world, "Material");
            ecs_entity_t mtlp_id = ecs_lookup(world, "MaterialTexPath");
            if (mat_id && mtlp_id) {
                ecs_query_desc_t qd = {};
                qd.terms[0].id = mat_id;
                qd.terms[1].id = mtlp_id;
                ecs_query_t* q = ecs_query_init(world, &qd);
                ecs_iter_t it = ecs_query_iter(world, q);
                while (ecs_query_next(&it)) {
                    for (int i=0;i<it.count;i++) {
                        MaterialData* m = (MaterialData*)ecs_get_id(world, it.entities[i], mat_id);
                        MaterialTexPath* mp = (MaterialTexPath*)ecs_get_id(world, it.entities[i], mtlp_id);
                        if (!m || !mp || !mp->path) continue;
                        if (m->tex != 0) continue;
                        std::string key(mp->path);
                        GLuint tex = 0;
                        auto itc = tex_cache.find(key);
                        if (itc != tex_cache.end()) {
                            tex = itc->second;
                        } else {
                            tex = load_texture_rgba8(key.c_str());
                            if (tex) tex_cache[key] = tex;
                        }
                        if (tex) {
                            m->tex = tex;
                            m->dirty = 1;
                            ecs_set_id(world, it.entities[i], mat_id, sizeof(MaterialData), m);
                            SDL_Log("[OBJ_EXAMPLE] Bound material texture %u to entity %llu (%s)", tex, (unsigned long long)it.entities[i], key.c_str());
                        } else {
                            SDL_Log("[OBJ_EXAMPLE] Failed to load texture %s", key.c_str());
                        }
                    }
                }
                ecs_query_fini(q);
            }
                    // Ensure Body component exists and get ids for relevant components
        ecs_entity_t body_id = ecs_lookup(world, "AmePhysicsBody");
        if (!body_id) {
            // Register via physics helper to guarantee correct layout
            (void)ame_physics_register_body_component(ameWorld);
            body_id = ecs_lookup(world, "AmePhysicsBody");
        }
        ecs_entity_t tr_id2 = ecs_lookup(world, "AmeTransform2D");
        if (!tr_id2) {
            (void)ame_physics_register_transform_component(ameWorld);
            tr_id2 = ecs_lookup(world, "AmeTransform2D");
        }
        SDL_Log("[PHYS] Transform component ID: %llu", (unsigned long long)tr_id2);
        ecs_entity_t col_id = ecs_lookup(world, "Collider2D");
        ecs_entity_t edge_id = ecs_lookup(world, "EdgeCollider2D");
        ecs_entity_t chain_id = ecs_lookup(world, "ChainCollider2D");
        ecs_entity_t meshcol_id = ecs_lookup(world, "MeshCollider2D");

        // Helper to ensure a body exists for an entity
        // Local PODs matching importer layouts
        struct Col2D { int type; float w,h; float radius; int isTrigger; int dirty; };
        struct MeshCol2D { const float* vertices; size_t count; int isTrigger; int dirty; };
        
        auto ensure_body_for_entity = [&](ecs_entity_t e){
            if (!body_id) { SDL_Log("[PHYS] body_id missing; cannot create body for %llu", (unsigned long long)e); return; }
            AmePhysicsBody* existing = (AmePhysicsBody*)ecs_get_id(world, e, body_id);
            if (existing && existing->body) { SDL_Log("[PHYS] entity %llu already has body %p", (unsigned long long)e, (void*)existing->body); return; }
            // Position
            AmeTransform2D tr = {0};
            if (tr_id2) {
                AmeTransform2D* trp = (AmeTransform2D*)ecs_get_id(world, e, tr_id2);
                if (trp) {
                    tr = *trp;
                    SDL_Log("[PHYS] entity %llu: read transform (%.2f,%.2f,%.2f)", (unsigned long long)e, tr.x, tr.y, tr.angle);
                } else {
                    tr.x = 0; tr.y = 0; tr.angle = 0; 
                    ecs_set_id(world, e, tr_id2, sizeof tr, &tr);
                    SDL_Log("[PHYS] entity %llu: no transform component, set default (0,0,0)", (unsigned long long)e);
                }
            } else {
                SDL_Log("[PHYS] entity %llu: tr_id2 not available", (unsigned long long)e);
            }
            // Default body box size; overridden by Collider2D or MeshCollider2D if present
            float bw = 0.1f, bh = 0.1f;
            bool is_sensor = false;
            int had_col = 0;
            if (col_id) {
                const Col2D* c = (const Col2D*)ecs_get_id(world, e, col_id);
                if (c) {
                    had_col = 1;
                    is_sensor = c->isTrigger != 0;
                    if (c->type == 0) { // Box
                        bw = (c->w > 0 ? c->w : bw);
                        bh = (c->h > 0 ? c->h : bh);
                    } else if (c->type == 1) { // Circle -> approximate as box
                        float d = c->radius * 2.0f;
                        if (d > 0) { bw = d; bh = d; }
                    }
                }
            }
            // Check for MeshCollider2D
            if (meshcol_id && had_col == 0) {
                const MeshCol2D* mc = (const MeshCol2D*)ecs_get_id(world, e, meshcol_id);
                if (mc && mc->vertices && mc->count >= 6) {
                    had_col = 2; // Mark as mesh collider
                    is_sensor = mc->isTrigger != 0;
                    // Compute bbox from mesh vertices for initial body size
                    float minx=1e9f,maxx=-1e9f,miny=1e9f,maxy=-1e9f;
                    for(size_t i=0; i<mc->count/2; i++) {
                        float x = mc->vertices[i*2+0], y = mc->vertices[i*2+1];
                        if(x<minx) minx=x; if(x>maxx) maxx=x;
                        if(y<miny) miny=y; if(y>maxy) maxy=y;
                    }
                    bw = (maxx-minx); bh = (maxy-miny);
                    if (bw <= 0) bw = 0.1f;
                    if (bh <= 0) bh = 0.1f;
                }
            }
            SDL_Log("[PHYS] create body for %llu tr=(%.2f,%.2f,%.2f) size=(%.2f,%.2f) sensor=%d had_col=%d", (unsigned long long)e, tr.x, tr.y, tr.angle, bw, bh, is_sensor?1:0, had_col);
            b2Body* body = ame_physics_create_body(physics, tr.x, tr.y, bw, bh, AME_BODY_STATIC, is_sensor, nullptr);
            if (body) {
                // Apply rotation if any
                if (tr.angle != 0.0f) {
                    ame_physics_set_angle(body, tr.angle);
                }
                AmePhysicsBody pb = {0};
                pb.body = body; pb.width = bw; pb.height = bh; pb.is_sensor = is_sensor;
                ecs_set_id(world, e, body_id, sizeof pb, &pb);
                SDL_Log("[PHYS] body created for %llu -> %p", (unsigned long long)e, (void*)body);
            } else {
                SDL_Log("[PHYS] ame_physics_create_body FAILED for %llu", (unsigned long long)e);
            }

        };

        // Query for Collider2D
        if (col_id) {
            ecs_query_desc_t qd = {};
            qd.terms[0].id = col_id;
            ecs_query_t* q = ecs_query_init(world, &qd);
            ecs_iter_t it = ecs_query_iter(world, q);
            int processed = 0;
            while (ecs_query_next(&it)) {
                for (int i=0;i<it.count;i++) { ensure_body_for_entity(it.entities[i]); processed++; }
            }
            SDL_Log("[PHYS] Collider2D entities processed: %d", processed);
            ecs_query_fini(q);
        }
        // Edge collider entities
        if (edge_id) {
            ecs_query_desc_t qd = {};
            qd.terms[0].id = edge_id;
            ecs_query_t* q = ecs_query_init(world, &qd);
            ecs_iter_t it = ecs_query_iter(world, q);
            while (ecs_query_next(&it)) {
                for (int i=0;i<it.count;i++) ensure_body_for_entity(it.entities[i]);
            }
            ecs_query_fini(q);
        }
        // Chain collider entities
        if (chain_id) {
            ecs_query_desc_t qd = {};
            qd.terms[0].id = chain_id;
            ecs_query_t* q = ecs_query_init(world, &qd);
            ecs_iter_t it = ecs_query_iter(world, q);
            while (ecs_query_next(&it)) {
                for (int i=0;i<it.count;i++) ensure_body_for_entity(it.entities[i]);
            }
            ecs_query_fini(q);
        }
        // Mesh collider entities
        if (meshcol_id) {
            ecs_query_desc_t qd = {};
            qd.terms[0].id = meshcol_id;
            ecs_query_t* q = ecs_query_init(world, &qd);
            ecs_iter_t it = ecs_query_iter(world, q);
            int processed = 0;
            while (ecs_query_next(&it)) {
                for (int i=0;i<it.count;i++) { ensure_body_for_entity(it.entities[i]); processed++; }
            }
            SDL_Log("[PHYS] MeshCollider2D entities processed: %d", processed);
            ecs_query_fini(q);
        }

        // No fallback physics body creation - only entities with collider components should get physics bodies

        obstaclesTotal = 7;
        obstaclesSpawned = 0;

        // Car root
        car = gameObject().scene()->Create("Car");
        auto* carCtl = &car.AddScript<CarController>();
        carCtl->SetPhysics(physics);
        carCtl->groundY = 0.0f;
        // Ensure car visuals use atlas and proper UVs (wheel: circle, body: solid)
        carCtl->ApplyAtlas(atlasTex, uvWheel, uvSolid);

        // Camera
        cameraObj = gameObject().scene()->Create("MainCamera");
        cameraCtl = &cameraObj.AddScript<CarCameraController>();
        cameraCtl->target = &car;
        cameraCtl->SetViewport(screenWidth, screenHeight);
        }
    }

    void FixedUpdate(float dt) {
        // Step the physics world
        if (physics) {
            ame_physics_world_step(physics);
            // Spawn one obstacle per frame until done
            b2World* w = (b2World*)physics->world;
            if (!w) return;
            if (obstaclesSpawned >= obstaclesTotal) return;

            int i = obstaclesSpawned;
            float x = -10.0f + i * 7.5f;
            float h = 1.0f + (i % 3) * 0.8f;
            float wHalf = 0.75f;
            float y = 0.5f + h * 0.5f;

            // physics body
            b2BodyDef bd; bd.type = b2_staticBody; bd.position.Set(x, y);
            b2Body* b = w->CreateBody(&bd);
            b2PolygonShape box; box.SetAsBox(wHalf, h*0.5f);
            b2FixtureDef fd; fd.shape = &box; fd.friction = 0.8f; b->CreateFixture(&fd);

            // visual
            GameObject ob = gameObject().scene()->Create("Obstacle");
            auto& sr = ob.AddComponent<SpriteRenderer>();
            sr.texture(atlasTex);
            sr.uv(uvNoise.x, uvNoise.y, uvNoise.z, uvNoise.w);
            sr.size({wHalf*2.0f, h});
            sr.sortingLayer(0); sr.orderInLayer(0); sr.z(0.0f);
            ob.transform().position({x, y, 0.0f});

            obstaclesSpawned++;
        }
    }

    void LateUpdate() {

    }

    void SetViewport(int w, int h) {
        screenWidth = w; screenHeight = h;
        if (cameraCtl) cameraCtl->SetViewport(w,h);
    }

    void OnDestroy() {
        if (physics) { ame_physics_world_destroy(physics); physics = nullptr; }
    }
};
