#pragma once
#include <stdbool.h>
#include <stdatomic.h>
#ifdef __cplusplus
extern "C" {
#endif

// Box2D-based physics; fixed 1000Hz step is managed internally on a dedicated thread.

bool physics_init(void);
void physics_shutdown(void);

// Threaded stepping starts implicitly in init and stops in shutdown

// Human and car bodies (opaque)
typedef struct b2Body b2Body;

b2Body* physics_create_dynamic_box(float x, float y, float w, float h, float density, float friction);
// Create a static box (e.g., ground/platform)
void physics_create_static_box(float x, float y, float w, float h, float friction);
void physics_apply_impulse(b2Body* body, float ix, float iy);
void physics_set_velocity(b2Body* body, float vx, float vy);
void physics_set_velocity_x(b2Body* body, float vx);
void physics_get_position(b2Body* body, float* x, float* y);
void physics_get_velocity(b2Body* body, float* vx, float* vy);

// Ground/contact helpers (default thresholds)
bool physics_is_grounded(b2Body* body);
// Configurable variant
bool physics_is_grounded_ex(b2Body* body, float normal_threshold, float max_upward_velocity);
// Teleport helper
void physics_teleport_body(b2Body* body, float x, float y);

// Expose gravity change, etc.
void physics_set_gravity(float gx, float gy);

// Access underlying Box2D world for advanced setups (wheels/joints)
struct b2World* physics_get_world(void);

#ifdef __cplusplus
}
#endif

