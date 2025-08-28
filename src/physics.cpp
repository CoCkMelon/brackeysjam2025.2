#include "physics.h"
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <stdlib.h>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <vector>
#include "ame/physics.h"

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
    b2Fixture* fx = b->CreateFixture(&fd);
#if defined(b2_api_H)
    FixtureUserData* ud = new FixtureUserData{true, false};
    b2FixtureUserData fud;
    fud.pointer = reinterpret_cast<uintptr_t>(ud);
    fx->SetUserData(fud);
#endif
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
    b2Fixture* fx = b->CreateFixture(&fd);
#if defined(b2_api_H)
    FixtureUserData* ud = new FixtureUserData{true, false};
    b2FixtureUserData fud;
    fud.pointer = reinterpret_cast<uintptr_t>(ud);
    fx->SetUserData(fud);
#endif
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
    b2Fixture* fx = b->CreateFixture(&fd);
#if defined(b2_api_H)
    FixtureUserData* ud = new FixtureUserData{true, false};
    b2FixtureUserData fud;
    fud.pointer = reinterpret_cast<uintptr_t>(ud);
    fx->SetUserData(fud);
#endif
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
        b2Fixture* fx = body->CreateFixture(&fd);
#if defined(b2_api_H)
        FixtureUserData* ud = new FixtureUserData{true, false};
        b2FixtureUserData fud;
        fud.pointer = reinterpret_cast<uintptr_t>(ud);
        fx->SetUserData(fud);
#endif
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
        if (my == b)
            n = -n;  // make normal point toward me
        if (n.y > normal_threshold) {
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }
    SDL_UnlockMutex(g_world_mtx);
    return false;
}

bool physics_is_grounded(b2Body* body) {
    if (!body)
        return false;
    // Use three short downward rays from the bottom of the body
    SDL_LockMutex(g_world_mtx);
    const float player_size = 10.0f;
    const float px = body->GetTransform().p.x;
    const float py = body->GetTransform().p.y;
    const float half = player_size * 0.5f;
    const float y0 = py - half - 1.0f;
    const float y1 = py - half - 12.0f;  // Y-up: cast downward from bottom of player
    const float ox[3] = {-half + 2.0f, 0.0f, half - 2.0f};
    struct LocalCB : public b2RayCastCallback {
        bool hit = false;
        b2Body* me = nullptr;
        float ReportFixture(b2Fixture* fixture,
                            const b2Vec2& p,
                            const b2Vec2& n,
                            float fr) override {
            (void)p;
            (void)n;
            if (fixture->IsSensor())
                return -1.0f;
            b2Body* b = fixture->GetBody();
            if (b == me)
                return -1.0f;
            if (b->GetType() == b2_dynamicBody)
                return -1.0f;
            hit = true;
            return fr;
        }
    } cb;
    cb.me = body;
    for (int i = 0; i < 3; ++i) {
        cb.hit = false;
        g_world->RayCast(&cb, b2Vec2(px + ox[i], y0), b2Vec2(px + ox[i], y1));
        if (cb.hit) {
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }
    SDL_UnlockMutex(g_world_mtx);
    return false;
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

bool physics_is_touching_wall(b2Body* body, int* out_dir) {
    if (out_dir)
        *out_dir = 0;
    if (!body)
        return false;

    SDL_LockMutex(g_world_mtx);

    // Build combined AABB of this body
    b2AABB aabb;
    aabb.lowerBound = b2Vec2(FLT_MAX, FLT_MAX);
    aabb.upperBound = b2Vec2(-FLT_MAX, -FLT_MAX);

    for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext()) {
#if defined(b2_api_H)
        int childCount = f->GetShape()->GetChildCount();
#else
        int childCount = 1;
#endif
        for (int i = 0; i < childCount; ++i) {
#if defined(b2_api_H)
            b2AABB fa = f->GetAABB(i);
#else
            b2AABB fa;
            fa.lowerBound = b2Vec2(0, 0);
            fa.upperBound = b2Vec2(0, 0);
#endif
            aabb.lowerBound.x = fminf(aabb.lowerBound.x, fa.lowerBound.x);
            aabb.lowerBound.y = fminf(aabb.lowerBound.y, fa.lowerBound.y);
            aabb.upperBound.x = fmaxf(aabb.upperBound.x, fa.upperBound.x);
            aabb.upperBound.y = fmaxf(aabb.upperBound.y, fa.upperBound.y);
        }
    }

    float cx = (aabb.lowerBound.x + aabb.upperBound.x) * 0.5f;
    float cy = (aabb.lowerBound.y + aabb.upperBound.y) * 0.5f;
    float hx = (aabb.upperBound.x - aabb.lowerBound.x) * 0.5f;
    float hy = (aabb.upperBound.y - aabb.lowerBound.y) * 0.5f;

    // Short raycast distance to check for immediate adjacency
    const float check_dist = 0.3f;

    // Sample points at different heights
    float y_samples[3] = {cy - hy * 0.4f, cy, cy + hy * 0.4f};

    struct LocalCB : public b2RayCastCallback {
        bool hit = false;
        b2Body* me = nullptr;
        float ReportFixture(b2Fixture* fixture,
                            const b2Vec2& p,
                            const b2Vec2& n,
                            float fr) override {
            (void)p;
            (void)n;
            if (fixture->IsSensor())
                return -1.0f;
            b2Body* b = fixture->GetBody();
            if (b == me)
                return -1.0f;
            if (b->GetType() == b2_dynamicBody)
                return -1.0f;  // ignore dynamic bodies
            hit = true;
            return fr;  // return the fraction to get the closest hit
        }
    } cb;
    cb.me = body;

    // Check right side - cast from body edge outward
    for (int i = 0; i < 3; ++i) {
        cb.hit = false;
        g_world->RayCast(&cb, b2Vec2(aabb.upperBound.x, y_samples[i]),
                         b2Vec2(aabb.upperBound.x + check_dist, y_samples[i]));
        if (cb.hit) {
            if (out_dir)
                *out_dir = 1;  // wall is to the right
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }

    // Check left side - cast from body edge outward
    for (int i = 0; i < 3; ++i) {
        cb.hit = false;
        g_world->RayCast(&cb, b2Vec2(aabb.lowerBound.x, y_samples[i]),
                         b2Vec2(aabb.lowerBound.x - check_dist, y_samples[i]));
        if (cb.hit) {
            if (out_dir)
                *out_dir = -1;  // wall is to the left
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }

    // Fallback: Check actual contacts for wall detection
    for (b2ContactEdge* ce = body->GetContactList(); ce; ce = ce->next) {
        b2Contact* c = ce->contact;
        if (!c->IsTouching())
            continue;

        b2Fixture* fixtureA = c->GetFixtureA();
        b2Fixture* fixtureB = c->GetFixtureB();
        b2Body* otherBody =
            (fixtureA->GetBody() == body) ? fixtureB->GetBody() : fixtureA->GetBody();

        // Skip if other body is dynamic (not a wall)
        if (otherBody->GetType() == b2_dynamicBody)
            continue;

        // Get contact normal
        b2WorldManifold wm;
        c->GetWorldManifold(&wm);
        b2Vec2 normal = wm.normal;

        // Make sure normal points toward our body
        if (fixtureA->GetBody() != body) {
            normal = -normal;
        }

        // Check if this is a vertical wall contact (horizontal normal)
        const float horizontal_threshold = 0.7f;
        if (fabsf(normal.x) > horizontal_threshold) {
            if (out_dir)
                *out_dir = (normal.x > 0.0f) ? 1 : -1;
            SDL_UnlockMutex(g_world_mtx);
            return true;
        }
    }

    SDL_UnlockMutex(g_world_mtx);
    return false;
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

void physics_add_sensor_box(b2Body* body, float w, float h, float offset_x, float offset_y) {
    if (!body)
        return;
    SDL_LockMutex(g_world_mtx);
    b2PolygonShape sh;
    sh.SetAsBox(w * 0.5f, h * 0.5f, b2Vec2(offset_x, offset_y), 0.0f);
    b2FixtureDef fd;
    fd.shape = &sh;
    fd.isSensor = true;
    fd.density = 0.0f;
    b2Fixture* fx = body->CreateFixture(&fd);
#if defined(b2_api_H)
    FixtureUserData* ud = new FixtureUserData{false, true};
    b2FixtureUserData fud;
    fud.pointer = reinterpret_cast<uintptr_t>(ud);
    fx->SetUserData(fud);
#endif
    SDL_UnlockMutex(g_world_mtx);
}

// Internal raycast callback to capture closest non-sensor hit
struct ClosestRayCastCB : public b2RayCastCallback {
    bool hit = false;
    float fraction_min = 1e9f;
    b2Vec2 point{};
    b2Vec2 normal{};
    b2Body* body = nullptr;
    float ReportFixture(b2Fixture* fixture,
                        const b2Vec2& point_,
                        const b2Vec2& normal_,
                        float fraction) override {
        if (fixture->IsSensor()) {
            return -1.0f;  // ignore sensors completely
        }
        if (fraction < fraction_min) {
            fraction_min = fraction;
            hit = true;
            point = point_;
            normal = normal_;
            body = fixture->GetBody();
        }
        return fraction;  // clip the ray to this point and continue to find closer
    }
};

RaycastCallback physics_raycast(float x0, float y0, float x1, float y1) {
    RaycastCallback out{};
    out.hit = false;
    out.x = out.y = out.nx = out.ny = 0.0f;
    out.fraction = 0.0f;
    out.body = nullptr;
    if (!g_world)
        return out;
    SDL_LockMutex(g_world_mtx);
    ClosestRayCastCB cb;
    g_world->RayCast(&cb, b2Vec2(x0, y0), b2Vec2(x1, y1));
    if (cb.hit) {
        out.hit = true;
        out.x = cb.point.x;
        out.y = cb.point.y;
        out.nx = cb.normal.x;
        out.ny = cb.normal.y;
        out.fraction = cb.fraction_min;
        out.body = cb.body;
    }
    SDL_UnlockMutex(g_world_mtx);
    return out;
}

void physics_lock(void) {
    if (g_world_mtx)
        SDL_LockMutex(g_world_mtx);
}
void physics_unlock(void) {
    if (g_world_mtx)
        SDL_UnlockMutex(g_world_mtx);
}
