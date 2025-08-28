#pragma once
#include <glad/gl.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

// Forward-only public API: avoid including C++ Box2D headers here so this can be consumed from C files.
// Implementation uses Box2D C++ and casts these opaque pointers appropriately.
typedef struct b2Body b2Body;        // opaque forward decl for C++ type
typedef struct b2WheelJoint b2WheelJoint; // opaque forward decl

typedef struct {
  // Visual size
  float body_w, body_h;
  // Wheel config
  float wheel_radius;
  float axle_offset_x;   // +/- from body center
  float suspension_hz;
  float suspension_damping;
  float motor_speed;     // base rad/s
  float motor_torque;    // max torque
  float jump_impulse;    // impulse for hop
  float boost_mul;       // multiplier when Shift held
} CarConfig;

typedef struct {
  // Box2D bodies (opaque to C callers)
  b2Body* body;
  b2Body* wheel_b;
  b2Body* wheel_f;
  b2WheelJoint* joint_b;
  b2WheelJoint* joint_f;
  // Config
  CarConfig cfg;
  // Simple textures (optional)
  GLuint tex_body;
  GLuint tex_wheel;
} Car;

void car_init(Car* c);
void car_shutdown(Car* c);
void car_fixed(Car* c, float dt);
void car_update(Car* c, float dt);
void car_render(const Car* c);
void car_set_position(Car* c, float x, float y);
void car_get_position(const Car* c, float* x, float* y);

#ifdef __cplusplus
}
#endif
