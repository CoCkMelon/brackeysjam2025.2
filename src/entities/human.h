#pragma once
#include <glad/gl.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct b2Body b2Body;

typedef struct {
    int tile_w;      // per-frame width in pixels
    int tile_h;      // per-frame height in pixels
    int row;         // which row to slice from spritesheet (0 = top)
    int idle;        // frame index for idle
    int walk[8];     // indices for walking cycle
    int walk_count;  // number of valid entries in walk[]
    int jump;        // frame index for jump
    float walk_fps;  // playback rate for walk
} HumanAnimConfig;

typedef struct {
    float max_hp;
    float hp;
} Health;

typedef struct {
    b2Body* body;
    int hidden;
    // Animation frames (textures) extracted from a single spritesheet row
    GLuint frames[32];
    int frame_count;      // how many frames were sliced from the row
    int current_frame;    // current frame index (0..frame_count-1)
    float anim_time;      // accumulation time for walk animation
    HumanAnimConfig cfg;  // all animation controls in one place
    // Size in world units/pixels (Y-up)
    float w, h;
    // Pending teleport request to be applied in human_fixed
    int pending_teleport;
    float pending_tx, pending_ty;
    // Temporarily disable horizontal control after a wall jump so impulse isn't overridden
    float x_control_lock;  // seconds remaining; when >0, horizontal velocity isn't forced
    // Health
    Health health;
} Human;

void human_init(Human* h);
void human_shutdown(Human* h);
void human_update(Human* h, float dt);  // render thread friendly: reads atomics only
void human_fixed(Human* h, float dt);   // logic thread 1000Hz
void human_render(const Human* h);
void human_set_position(Human* h, float x, float y);
void human_get_position(const Human* h, float* x, float* y);
void human_hide(Human* h, bool hide);

// Health helpers
void human_apply_damage(Human* h, float dmg);

#ifdef __cplusplus
}
#endif
