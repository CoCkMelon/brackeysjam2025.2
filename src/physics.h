#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

// Box2D-based physics; fixed 1000Hz step is managed internally on a dedicated thread.

bool physics_init(void);
void physics_shutdown(void);

// Threaded stepping starts implicitly in init and stops in shutdown

// Human and car bodies (opaque)
typedef struct b2Body b2Body;

b2Body* physics_create_dynamic_box(float x,
                                   float y,
                                   float w,
                                   float h,
                                   float density,
                                   float friction);
// Static colliders
void physics_create_static_box(float x, float y, float w, float h, float friction);
void physics_create_static_circle(float x, float y, float r, float friction);
void physics_create_static_edge(float x1, float y1, float x2, float y2, float friction);
void physics_create_static_chain(const float* xy_pairs, int count, bool loop, float friction);
// Triangles array: pos points are grouped as triangles (3 vertices per tri)
void physics_create_static_mesh_triangles(const float* pos, int vertex_count, float friction);
// Tagged variant: flags bitfield applied to created fixtures (for hazards like spikes)
void physics_create_static_mesh_triangles_tagged(const float* pos, int vertex_count, float friction, int flags);
// Kinematic helpers
b2Body* physics_create_kinematic_circle(float x, float y, float r, float friction);
void physics_apply_impulse(b2Body* body, float ix, float iy);
void physics_set_velocity(b2Body* body, float vx, float vy);
void physics_set_velocity_x(b2Body* body, float vx);
void physics_get_position(b2Body* body, float* x, float* y);
void physics_get_velocity(b2Body* body, float* vx, float* vy);
void physics_set_angular_velocity(b2Body* body, float av);
float physics_get_angle(b2Body* body);

// Ground/contact helpers (default thresholds)
bool physics_is_grounded(b2Body* body);
// Configurable variant
bool physics_is_grounded_ex(b2Body* body, float normal_threshold, float max_upward_velocity);
// Detect if body is touching a wall via a sensor; out_dir will be -1 for wall on left, +1 for wall
// on right
bool physics_is_touching_wall(b2Body* body, int* out_dir);

// Gameplay flags for fixtures
#define PHYS_FLAG_SPIKE (1<<0)

// Simple raycast result used by gameplay helpers
typedef struct RaycastCallback {
    bool hit;        // whether anything was hit
    float x, y;      // hit point (world)
    float nx, ny;    // surface normal
    float fraction;  // ray fraction along [0,1]
    b2Body* body;    // body that was hit (if any)
} RaycastCallback;

// Cast a ray in world space and return the closest solid hit (sensors are ignored)
RaycastCallback physics_raycast(float x0, float y0, float x1, float y1);

// Add a sensor rectangle fixture to a body (for feet or side sensors). Offset is relative to body
// center.
void physics_add_sensor_box(b2Body* body, float w, float h, float offset_x, float offset_y);

// Teleport helper
void physics_teleport_body(b2Body* body, float x, float y);
// Enable/disable a body (disables all its fixtures when false)
void physics_set_body_enabled(b2Body* body, bool enabled);
// Query if an axis-aligned box overlaps any fixture in the world, ignoring up to two bodies
bool physics_overlap_aabb(float cx, float cy, float w, float h, b2Body* ignore_a, b2Body* ignore_b);
// Returns true if body is currently touching any fixture that has all bits in required_flags set
bool physics_body_touching_flag(b2Body* body, int required_flags);
// Returns true if touching a flagged fixture and outputs an approximate max contact-point speed
bool physics_body_touching_flag_speed(b2Body* body, int required_flags, float* out_max_speed);
// Returns true if two bodies are currently contacting (non-sensor)
bool physics_bodies_touching(b2Body* a, b2Body* b);
// Returns true if two bodies are touching and outputs an approximate max relative contact speed
bool physics_bodies_contact_speed(b2Body* a, b2Body* b, float* out_max_speed);

// Expose gravity change, etc.
void physics_set_gravity(float gx, float gy);

// Access underlying Box2D world for advanced setups (wheels/joints)
struct b2World* physics_get_world(void);

// Thread-safety helpers: acquire before directly using Box2D and release after
void physics_lock(void);
void physics_unlock(void);

#ifdef __cplusplus
}
#endif
