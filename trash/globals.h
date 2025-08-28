#pragma once
// Fixed timestep
#include <SDL3/SDL_stdinc.h>
extern "C" {
#include "ame/ecs.h"
#include "ame/render_pipeline_ecs.h"
}
static const float fixedTimeStep = 1.0f / 1000.0f;
static float accumulator = 0.0f;
static Uint64 lastTime = 0;
static int windowWidth = 1280;
static int windowHeight = 720;
static AmeEcsWorld* ameWorld = nullptr;
static Scene* scene = nullptr;
ecs_world_t* world = nullptr;
