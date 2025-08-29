#pragma once
#include <glad/gl.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Centralized asset-dependent configuration for the Human entity
// Update these if the spritesheet or layout changes.
#define HUMAN_SPRITESHEET_PATH        "assets/HumanCombined.png"
#define HUMAN_TILE_W                  22   // per-frame width in pixels
#define HUMAN_TILE_H                  27   // per-frame height in pixels
#define HUMAN_SPRITESHEET_ROW         0    // which row to slice (0 = top)

// Frames layout in HumanCombined.png (single row, 22px tiles):
// index 0        : idle/stand
// indices 1..4   : walk cycle (4 frames)
// indices 5..11  : jump sequence (7 frames)
#define HUMAN_IDLE_FRAME_INDEX        0
#define HUMAN_WALK_FRAME_COUNT        4
#define HUMAN_WALK_FRAME_0            1
#define HUMAN_WALK_FRAME_1            2
#define HUMAN_WALK_FRAME_2            3
#define HUMAN_WALK_FRAME_3            4
#define HUMAN_WALK_FPS                10.0f
#define HUMAN_JUMP_FIRST_INDEX        5
#define HUMAN_JUMP_FRAME_COUNT        7
#define HUMAN_JUMP_FPS                10.0f

typedef struct b2Body b2Body;

typedef struct {
    int tile_w;      // per-frame width in pixels
    int tile_h;      // per-frame height in pixels
    int row;         // which row to slice from spritesheet (0 = top)
    int idle;        // frame index for idle
    int walk[8];     // indices for walking cycle
    int walk_count;  // number of valid entries in walk[]
    int jump_first;  // first index for jump sequence
    int jump_count;  // number of jump frames
    float walk_fps;  // playback rate for walk
    float jump_fps;  // playback rate for jump
} HumanAnimConfig;

static inline HumanAnimConfig human_default_anim_config(void) {
    HumanAnimConfig c;
    c.tile_w = HUMAN_TILE_W;
    c.tile_h = HUMAN_TILE_H;
    c.row = HUMAN_SPRITESHEET_ROW;
    c.idle = HUMAN_IDLE_FRAME_INDEX;
    c.walk[0] = HUMAN_WALK_FRAME_0;
    c.walk[1] = HUMAN_WALK_FRAME_1;
    c.walk[2] = HUMAN_WALK_FRAME_2;
    c.walk[3] = HUMAN_WALK_FRAME_3;
    c.walk_count = HUMAN_WALK_FRAME_COUNT;
    c.jump_first = HUMAN_JUMP_FIRST_INDEX;
    c.jump_count = HUMAN_JUMP_FRAME_COUNT;
    c.walk_fps = HUMAN_WALK_FPS;
    c.jump_fps = HUMAN_JUMP_FPS;
    return c;
}

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
    float anim_time;      // accumulation time for animation (walk)
    HumanAnimConfig cfg;  // all animation controls in one place
    // Facing: +1 = right, -1 = left
    int facing;
    // Grounded state tracking for animation resets
    bool was_grounded;
    // Jump takeoff one-shot animation state (2 frames)
    int jump_anim_playing;   // 1 while playing the 2-frame takeoff
    float jump_anim_time;    // time accumulator for jump takeoff
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
