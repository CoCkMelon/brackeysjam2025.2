#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

struct AmeCamera;  // fwd
struct AmeAudioSource;
struct AmeAudioSourceRef;

#include "entities/car.h"
#include "entities/human.h"

typedef struct GameplayTriggerUser {
    float x;
    float y;
} GameplayTriggerUser;

bool gameplay_init(void);
void gameplay_shutdown(void);

// Fixed-step simulation (1000 Hz like physics thread)
void gameplay_fixed(Human* human, Car* car, float dt);
// Frame update for audio panning and cleanup
void gameplay_update(Human* human,
                     Car* car,
                     float cam_x,
                     float cam_y,
                     float viewport_w_pixels,
                     float zoom,
                     float dt);

// Collect current gameplay audio refs (explosions, saws, etc.). Returns count.
// 'out' must have capacity for at least max_refs entries.
int gameplay_collect_audio_refs(struct AmeAudioSourceRef* out,
                                int max_refs,
                                float cam_x,
                                float cam_y,
                                float viewport_w_pixels,
                                float zoom,
                                float dt);
// Draw simple sprites for objects
void gameplay_render(void);

// Spawns using world-space positions (Y-up)
void spawn_grenade(float x, float y, float vx, float vy, float fuse_sec);
void spawn_mine(float x, float y);
void spawn_turret(float x, float y);
void spawn_rocket(float x, float y, float vx, float vy, float life_sec);
void spawn_fuel_pickup(float x, float y, float amount);

// Trigger integration
void gameplay_on_trigger(const char* name, void* user);

// Spawn points
void gameplay_add_spawn_point(float x, float y);

// Restart helpers
void gameplay_restart(Human* human, Car* car);

// (Spike hazards handled via collider flags; no AABB registration)

// Global gameplay tuning constants (easy to tweak)
// Base DPS when standing on a saw (continuous). Spikes have no base DPS.
#define GAME_SAW_BASE_DPS_HUMAN 80.0f
#define GAME_SAW_BASE_DPS_CAR 50.0f
// Additional saw impact damage parameters
#define GAME_SAW_IMPACT_THRESH 6.0f
#define GAME_SAW_IMPACT_SCALE_HUM 4.0f
#define GAME_SAW_IMPACT_SCALE_CAR 3.0f

// Explosive damage (radial)
#define GAME_GRENADE_DAMAGE 40.0f
#define GAME_MINE_DAMAGE 50.0f
#define GAME_ROCKET_DAMAGE 35.0f

// Turret hitscan shot
#define GAME_TURRET_SHOT_DAMAGE 20.0f

// Spike impact parameters (no base DPS)
#define GAME_SPIKE_HUMAN_THRESH 7.0f
#define GAME_SPIKE_HUMAN_SCALE 3.0f
#define GAME_SPIKE_CAR_THRESH 8.0f
#define GAME_SPIKE_CAR_SCALE 2.0f

// Saw entity (circle collider that spins fast)
void gameplay_spawn_saw(float x, float y, float radius);

// Spawn activation proximity radius (when car is near a spawn point it becomes active)
#ifndef GAME_SPAWN_ACTIVATE_RADIUS
#define GAME_SPAWN_ACTIVATE_RADIUS 1000.0f
#endif

#ifdef __cplusplus
}
#endif
