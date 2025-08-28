#include "physics.h"
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <stdlib.h>
#include <atomic>
#include <cmath>
#include <vector>

struct FixtureUserData {
    bool is_ground;
    bool is_sensor;
};

static b2World* g_world = nullptr;
static SDL_Thread* g_thread = NULL;
static std::atomic_bool g_running{false};
static float g_dt = 0.001f;  // 1000 Hz
static SDL_Mutex* g_world_mtx = NULL;

static int physics_thread(void* ud) {
    (void)ud;
    uint64_t last = SDL_GetTicksNS();
    double acc = 0.0;
    g_running.store(true);
    while (g_running.load()) {
        uint64_t t = SDL_GetTicksNS();
        double frame = (double)(t - last) / 1e9;
        last = t;
        if (frame > 0.05)
            frame = 0.05;
        acc += frame;
        int steps = 0;
        while (acc >= g_dt && steps < 8) {
            if (g_world) {
                SDL_LockMutex(g_world_mtx);
                g_world->Step(g_dt, 8, 3);
                SDL_UnlockMutex(g_world_mtx);
            }
            acc -= g_dt;
            steps++;
        }
        SDL_DelayNS(200000);  // ~0.2ms
    }
    return 0;
}

bool physics_init(void) {
    b2Vec2 gravity(0.0f, -100.0f);
    g_world = new b2World(gravity);
    g_world_mtx = SDL_CreateMutex();
    g_thread = SDL_CreateThread(physics_thread, "phys", NULL);
    return g_world && g_thread && g_world_mtx;
}

void physics_shutdown(void) {
    g_running.store(false);
    if (g_thread) {
        SDL_WaitThread(g_thread, NULL);
        g_thread = NULL;
    }
    if (g_world_mtx) {
        SDL_DestroyMutex(g_world_mtx);
        g_world_mtx = NULL;
    }
    delete g_world;
    g_world = nullptr;
}

b2Body* physics_create_dynamic_box(float x,
                                   float y,
                                   float w,
                                   float h,
                                   float density,
                                   float friction) {
    if (!g_world)
        return nullptr;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_dynamicBody;
    bd.position.Set(x, y);
    b2Body* b = g_world->CreateBody(&bd);
    b2PolygonShape sh;
    sh.SetAsBox(w * 0.5f, h * 0.5f);
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.density = density;
    fd.friction = friction;
    b->CreateFixture(&fd);
    SDL_UnlockMutex(g_world_mtx);
    return b;
}

void physics_create_static_box(float x, float y, float w, float h, float friction) {
    if (!g_world)
        return;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_staticBody;
    bd.position.Set(x, y);
    b2Body* b = g_world->CreateBody(&bd);
    b2PolygonShape sh;
    sh.SetAsBox(w * 0.5f, h * 0.5f);
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.friction = friction;
    b->CreateFixture(&fd);
    SDL_UnlockMutex(g_world_mtx);
}

void physics_create_static_circle(float x, float y, float r, float friction) {
    if (!g_world)
        return;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_staticBody;
    bd.position.Set(x, y);
    b2Body* b = g_world->CreateBody(&bd);
    b2CircleShape sh;
    sh.m_p.Set(0, 0);
    sh.m_radius = r;
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.friction = friction;
    b->CreateFixture(&fd);
    SDL_UnlockMutex(g_world_mtx);
}

void physics_create_static_edge(float x1, float y1, float x2, float y2, float friction) {
    if (!g_world)
        return;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* b = g_world->CreateBody(&bd);
    b2EdgeShape sh;
    sh.SetTwoSided(b2Vec2(x1, y1), b2Vec2(x2, y2));
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.friction = friction;
    b->CreateFixture(&fd);
    SDL_UnlockMutex(g_world_mtx);
}

void physics_create_static_chain(const float* xy_pairs, int count, bool loop, float friction) {
    if (!g_world || !xy_pairs || count < 2)
        return;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* b = g_world->CreateBody(&bd);
    std::vector<b2Vec2> pts;
    pts.reserve((size_t)count);
    for (int i = 0; i < count; i++) {
        pts.emplace_back(xy_pairs[i * 2 + 0], xy_pairs[i * 2 + 1]);
    }
    b2ChainShape sh;
    if (loop)
        sh.CreateLoop(pts.data(), (int)pts.size());
    else
        sh.CreateChain(pts.data(), (int)pts.size(), pts[0], pts[pts.size() - 1]);
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.friction = friction;
    b->CreateFixture(&fd);
    SDL_UnlockMutex(g_world_mtx);
}

static bool is_triangle_valid(const b2Vec2* v) {
    // Check for zero area (cross product)
    b2Vec2 a = v[1] - v[0];
    b2Vec2 b = v[2] - v[0];
    float cross = a.x * b.y - a.y * b.x;
    if (fabs(cross) < 1e-6f)
        return false;  // Degenerate triangle

    // Check for duplicate vertices
    const float eps = 1e-6f;
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            b2Vec2 diff = v[i] - v[j];
            if (diff.LengthSquared() < eps * eps)
                return false;
        }
    }

    return true;
}

void physics_create_static_mesh_triangles(const float* pos, int vertex_count, float friction) {
    if (!g_world || !pos || vertex_count < 3)
        return;
    SDL_LockMutex(g_world_mtx);
    b2BodyDef bd;
    bd.type = b2_staticBody;
    b2Body* body = g_world->CreateBody(&bd);
    for (int i = 0; i + 2 < vertex_count; i += 3) {
        b2Vec2 v[3] = {b2Vec2(pos[(i + 0) * 2 + 0], pos[(i + 0) * 2 + 1]),
                       b2Vec2(pos[(i + 1) * 2 + 0], pos[(i + 1) * 2 + 1]),
                       b2Vec2(pos[(i + 2) * 2 + 0], pos[(i + 2) * 2 + 1])};

        // Validate triangle before creating Box2D shape
        if (!is_triangle_valid(v))
            continue;

        // Ensure counter-clockwise winding
        b2Vec2 a = v[1] - v[0];
        b2Vec2 b = v[2] - v[0];
        float cross = a.x * b.y - a.y * b.x;
        if (cross < 0) {
            // Swap vertices 1 and 2 to fix winding
            b2Vec2 temp = v[1];
            v[1] = v[2];
            v[2] = temp;
        }

        b2PolygonShape sh;
        sh.Set(v, 3);
        b2FixtureDef fd;
        fd.shape = &sh;
        fd.friction = friction;
        body->CreateFixture(&fd);
    }
    SDL_UnlockMutex(g_world_mtx);
}

void physics_apply_impulse(b2Body* body, float ix, float iy) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    body->ApplyLinearImpulseToCenter(b2Vec2(ix, iy), true);
    SDL_UnlockMutex(g_world_mtx);
}
void physics_set_velocity(b2Body* body, float vx, float vy) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    body->SetLinearVelocity(b2Vec2(vx, vy));
    SDL_UnlockMutex(g_world_mtx);
}
void physics_set_velocity_x(b2Body* body, float vx) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    b2Vec2 v = body->GetLinearVelocity();
    v.x = vx;
    body->SetLinearVelocity(v);
    SDL_UnlockMutex(g_world_mtx);
}
void physics_get_position(b2Body* body, float* x, float* y) {
    if (!body) {
        if (x)
            *x = 0;
        if (y)
            *y = 0;
        return;
    }
    SDL_LockMutex(g_world_mtx);
    b2Vec2 p = body->GetPosition();
    if (x)
        *x = p.x;
    if (y)
        *y = p.y;
    SDL_UnlockMutex(g_world_mtx);
}
void physics_get_velocity(b2Body* body, float* vx, float* vy) {
    if (!body) {
        if (vx)
            *vx = 0;
        if (vy)
            *vy = 0;
        return;
    }
    SDL_LockMutex(g_world_mtx);
    b2Vec2 v = body->GetLinearVelocity();
    if (vx)
        *vx = v.x;
    if (vy)
        *vy = v.y;
    SDL_UnlockMutex(g_world_mtx);
}

bool physics_is_grounded_ex(b2Body* body, float normal_threshold, float max_upward_velocity) {
    if (!body)
        return false;
    SDL_LockMutex(g_world_mtx);
    b2Vec2 vel = body->GetLinearVelocity();
    if (vel.y > max_upward_velocity) {
        SDL_UnlockMutex(g_world_mtx);
        return false;
    }
    for (b2ContactEdge* ce = body->GetContactList(); ce; ce = ce->next) {
        b2Contact* c = ce->contact;
        if (!c->IsTouching())
            continue;
        b2Fixture* a = c->GetFixtureA();
        b2Fixture* b = c->GetFixtureB();
        b2Fixture* my = (a->GetBody() == body) ? a : b;
        b2Fixture* other = (a->GetBody() == body) ? b : a;
#if defined(b2_api_H)
        // Box2D C++ 2.4 user data API
        FixtureUserData* my_ud = reinterpret_cast<FixtureUserData*>(my->GetUserData().pointer);
        FixtureUserData* ot_ud = reinterpret_cast<FixtureUserData*>(other->GetUserData().pointer);
#else
        FixtureUserData* my_ud = NULL;
        FixtureUserData* ot_ud = NULL;
#endif
        if (!my_ud || !my_ud->is_sensor)
            continue;
        if (!ot_ud || !ot_ud->is_ground)
            continue;
        b2WorldManifold wm;
        c->GetWorldManifold(&wm);
        b2Vec2 n = wm.normal;
        if (n.y > normal_threshold) {
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }
    SDL_UnlockMutex(g_world_mtx);
    return false;
}

bool physics_is_grounded(b2Body* body) {
    return physics_is_grounded_ex(body, 0.5f, 0.0f);
}

void physics_teleport_body(b2Body* body, float x, float y) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    body->SetTransform(b2Vec2(x, y), body->GetAngle());
    body->SetLinearVelocity(b2Vec2(0, 0));
    body->SetAngularVelocity(0);
    SDL_UnlockMutex(g_world_mtx);
}

void physics_set_gravity(float gx, float gy) {
    if (!g_world)
        return;
    SDL_LockMutex(g_world_mtx);
    g_world->SetGravity(b2Vec2(gx, gy));
    SDL_UnlockMutex(g_world_mtx);
}

void physics_set_body_enabled(b2Body* body, bool enabled) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    body->SetEnabled(enabled);
    SDL_UnlockMutex(g_world_mtx);
}

struct OverlapQueryCB : public b2QueryCallback {
    b2Body* a{nullptr};
    b2Body* b{nullptr};
    bool hit{false};
    bool ReportFixture(b2Fixture* fixture) override {
        b2Body* fb = fixture->GetBody();
        if (fb == a || fb == b)
            return true;  // continue
        hit = true;
        return false;  // stop early
    }
};

bool physics_overlap_aabb(float cx,
                          float cy,
                          float w,
                          float h,
                          b2Body* ignore_a,
                          b2Body* ignore_b) {
    if (!g_world)
        return false;
    SDL_LockMutex(g_world_mtx);
    OverlapQueryCB cb;
    cb.a = ignore_a;
    cb.b = ignore_b;
    b2AABB box;
    float hx = w * 0.5f, hy = h * 0.5f;
    box.lowerBound.Set(cx - hx, cy - hy);
    box.upperBound.Set(cx + hx, cy + hy);
    g_world->QueryAABB(&cb, box);
    bool res = cb.hit;
    SDL_UnlockMutex(g_world_mtx);
    return res;
}

b2World* physics_get_world(void) {
    return g_world;
}

void physics_lock(void) {
    if (g_world_mtx)
        SDL_LockMutex(g_world_mtx);
}
void physics_unlock(void) {
    if (g_world_mtx)
        SDL_UnlockMutex(g_world_mtx);
}
