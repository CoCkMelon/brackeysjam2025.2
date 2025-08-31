#include "app.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "abilities.h"
#include "ame/audio.h"
#include "ame/camera.h"
#include "ame_dialogue.h"
#include "config.h"
#include "dialogue_generated.h"
#include "dialogue_manager.h"
#include "entities/car.h"
#include "entities/human.h"
#include "gameplay.h"
#include "input.h"
#include "obj_map.h"
#include "path_util.h"
#include "physics.h"
#include "render/pipeline.h"
#include "triggers.h"
#include "ui.h"

static SDL_Window* g_window = NULL;
static SDL_GLContext g_gl = NULL;
static int g_w = APP_DEFAULT_WIDTH, g_h = APP_DEFAULT_HEIGHT;

static AmeCamera g_cam;
static atomic_bool g_should_quit = false;

// Audio
static AmeAudioSource g_music;
static AmeAudioSource g_car_rear_audio;   // rear wheel/motor
static AmeAudioSource g_car_front_audio;  // front wheel/motor
static AmeAudioSource g_ball_audio;
static uint64_t g_music_id = 1, g_car_rear_audio_id = 2, g_ball_audio_id = 3,
                g_car_front_audio_id = 4;

// Simple ball for spatial audio demo
static b2Body* g_ball_body = NULL;
static AmeLocalMesh g_map_mesh = {0};

static SDL_Thread* g_logic_thread = NULL;
static atomic_bool g_logic_running = false;
static const float kFixedDt = APP_FIXED_DT;  // from config

// #define MAP_OBJ_NAME "test dimensions.obj"
#define MAP_OBJ_NAME APP_MAP_OBJ_NAME

static void set_viewport(int w, int h) {
    glViewport(0, 0, w, h);
    ame_camera_set_viewport(&g_cam, w, h);
}

static int init_gl(void) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    g_window =
        SDL_CreateWindow(APP_WINDOW_TITLE, g_w, g_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        SDL_Log("window: %s", SDL_GetError());
        return 0;
    }
    g_gl = SDL_GL_CreateContext(g_window);
    if (!g_gl) {
        SDL_Log("gl ctx: %s", SDL_GetError());
        return 0;
    }
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
        SDL_Log("glad fail");
        return 0;
    }
    // Ensure backface culling is disabled globally
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Init UI subsystem (fonts)
    ui_init();

    set_viewport(g_w, g_h);
    return 1;
}

static void shutdown_gl(void) {
    if (g_gl) {
        SDL_GL_DestroyContext(g_gl);
        g_gl = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }
}

static int logic_thread_main(void* ud) {
    (void)ud;
    uint64_t last = SDL_GetTicksNS();
    double acc = 0.0;
    atomic_store(&g_logic_running, true);
    while (!atomic_load(&g_should_quit)) {
        uint64_t t = SDL_GetTicksNS();
        double frame = (double)(t - last) / 1e9;
        last = t;
        if (frame > 0.05)
            frame = 0.05;
        acc += frame;
        int steps = 0;
        while (acc >= kFixedDt && steps < 8) {
            input_update();
            if (g_mode == CONTROL_CAR) {
                car_fixed(&g_car, kFixedDt);
            } else {
                human_fixed(&g_human, kFixedDt);
            }
            // Gameplay fixed-step (weapons, timers)
            gameplay_fixed(&g_human, &g_car, kFixedDt);
            acc -= kFixedDt;
            steps++;
        }
        SDL_DelayNS(200000);  // ~0.2 ms
    }
    atomic_store(&g_logic_running, false);
    return 0;
}

static void update_switch_logic(void) {
    if (input_pressed_switch()) {
        float hx, hy, cx, cy;
        const float switch_threshold = 64;
        human_get_position(&g_human, &hx, &hy);
        car_get_position(&g_car, &cx, &cy);
        float dx = hx - cx, dy = hy - cy;
        float d2 = dx * dx + dy * dy;
        SDL_Log("Switch pressed: distance=%.2f, threshold=%.2f", sqrtf(d2), switch_threshold);
        if (d2 < switch_threshold * switch_threshold || g_mode == CONTROL_CAR) {
            if (g_mode == CONTROL_HUMAN) {
                SDL_Log("Switching from HUMAN to CAR");
                human_hide(&g_human, true);
                human_set_position(&g_human, cx, cy);
                g_mode = CONTROL_CAR;
            } else {
                SDL_Log("Switching from CAR to HUMAN");
                human_set_position(&g_human, cx, cy);
                human_hide(&g_human, false);
                g_mode = CONTROL_HUMAN;
            }
        } else {
            SDL_Log("Too far to switch (distance: %.2f)", sqrtf(d2));
        }
    }
}

static void on_trigger_unlock(const char* name, void* user) {
    (void)user;
    if (!name)
        return;
    if (SDL_strcmp(name, "unlock_car_jump") == 0) {
        car_set_jump(true);
        SDL_Log("Ability unlocked: car_jump");
    } else if (SDL_strcmp(name, "unlock_car_boost") == 0) {
        car_set_boost(true);
        SDL_Log("Ability unlocked: car_boost");
    } else if (SDL_strcmp(name, "unlock_car_fly") == 0) {
        car_set_fly(true);
        SDL_Log("Ability unlocked: car_fly");
    }
}

int game_app_init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 0;
    if (!init_gl())
        return 0;
    ame_camera_init(&g_cam);
    g_cam.zoom = APP_DEFAULT_ZOOM;
    ame_camera_set_viewport(&g_cam, g_w, g_h);
    if (!input_init())
        return 0;
    if (!physics_init())
        return 0;  // physics thread (1000Hz)
    // Cache base path once for all asset lookups
    pathutil_init();
    if (!pipeline_init())
        return 0;
    if (!ame_audio_init(48000))
        return 0;
    if (!gameplay_init())
        return 0;

    // Init dialogue manager and optionally start an introduction scene
    dialogue_manager_init();

    // Music (looping)
    if (!ame_audio_source_load_opus_file(&g_music, APP_MUSIC_PATH, true)) {
        SDL_Log("music load failed");
    }
    g_music.gain = 0.35f;
    g_music.pan = 0.0f;
    g_music.playing = true;

    // Car engine hums (non-spatial)
    // 8-bit style: Higher shape (10-15) for square-like waves, lower frequencies for motor rumble
    ame_audio_source_init_sigmoid(&g_car_rear_audio, 55.0f, 12.0f, 0.15f);
    g_car_rear_audio.pan = 0.0f;
    // Front wheel motor - slightly different for layering effect
    ame_audio_source_init_sigmoid(&g_car_front_audio, 65.0f, 10.0f, 0.12f);
    g_car_front_audio.pan = 0.0f;

    // Ball rolling tone (spatial by pan)
    ame_audio_source_init_sigmoid(&g_ball_audio, 220.0f, 6.0f, 0.12f);
    g_ball_audio.pan = 0.0f;
    abilities_init();
    triggers_init();

    // Register dialogue-driven triggers (manual fire via dialogue_trigger_hook)
    Aabb dummy = {0, 0, 0, 0};
    triggers_add("unlock_car_jump", dummy, 1, on_trigger_unlock, NULL);
    triggers_add("unlock_car_boost", dummy, 1, on_trigger_unlock, NULL);
    triggers_add("unlock_car_fly", dummy, 1, on_trigger_unlock, NULL);

    // Optionally auto-start a default scene if available
    dialogue_start_scene("introduction");

    human_init(&g_human);
    car_init(&g_car);

    // Create a small "ball" as a dynamic box to demonstrate spatial audio
    g_ball_body = physics_create_dynamic_box(200.0f, 150.0f, 6.0f, 6.0f, 0.5f, 0.6f);
    physics_set_velocity(g_ball_body, 30.0f, 0.0f);

    // Load map from OBJ (creates static colliders and optional visuals)
    {
        const char* base = pathutil_base();
        const char* candidates[6];
        int ci = 0;
        // Prefer paths relative to executable
        static char p0[1024];
        static char p1[1024];
        if (base && base[0]) {
            SDL_snprintf(p0, sizeof(p0), "%sassets/" MAP_OBJ_NAME, base);
            candidates[ci++] = p0;
        }
        if (base && base[0]) {
            SDL_snprintf(p1, sizeof(p1), "%s../assets/" MAP_OBJ_NAME, base);
            candidates[ci++] = p1;
        }
        // Also try common relative paths from CWD
        candidates[ci++] = "assets/" MAP_OBJ_NAME;
        candidates[ci++] = "../assets/" MAP_OBJ_NAME;
        candidates[ci++] = "./assets/" MAP_OBJ_NAME;
        candidates[ci++] = NULL;
        bool ok = false;
        for (int i = 0; i < ci && candidates[i]; ++i) {
            if (load_obj_map(candidates[i], &g_map_mesh)) {
                SDL_Log("Loaded map: %s (%u verts)", candidates[i], g_map_mesh.count);
                ok = true;
                break;
            }
        }
        if (!ok) {
            SDL_Log(
                "Failed to load map car_village.obj after trying executable-relative and "
                "working-directory paths");
        }
    }

    // Spawn points.
    gameplay_add_spawn_point(APP_DEFAULT_SPAWN_X, APP_DEFAULT_SPAWN_Y);  // default spawn
    car_set_position(&g_car, APP_START_CAR_X, APP_START_CAR_Y);
    human_set_position(&g_human, APP_START_HUMAN_X, APP_START_HUMAN_Y);

    g_logic_thread = SDL_CreateThread(logic_thread_main, "logic", NULL);
    if (!g_logic_thread) {
        SDL_Log("failed to start logic thread: %s", SDL_GetError());
        return 0;
    }
    return 1;
}

int game_app_event(void* appstate, void* evp) {
    (void)appstate;
    SDL_Event* event = (SDL_Event*)evp;
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    if (event->type == SDL_EVENT_WINDOW_RESIZED &&
        event->window.windowID == SDL_GetWindowID(g_window)) {
        g_w = event->window.data1;
        g_h = event->window.data2;
        set_viewport(g_w, g_h);
    }
    return SDL_APP_CONTINUE;
}

int game_app_iterate(void* appstate) {
    (void)appstate;
    static uint64_t prev = 0;
    uint64_t t = SDL_GetTicksNS();
    if (prev == 0)
        prev = t;
    float dt = (float)((t - prev) / 1e9);
    prev = t;

    // Check for quit request
    if (input_quit_requested()) {
        return SDL_APP_SUCCESS;
    }

    update_switch_logic();

    // Restart on R
    if (input_restart_edge()) {
        gameplay_restart(&g_human, &g_car);
    }

    // Keep human near car when driving to allow quick switch back
    if (g_mode == CONTROL_CAR) {
        float cx, cy;
        car_get_position(&g_car, &cx, &cy);
        human_set_position(&g_human, cx, cy);
    }

    // Dialogue input handling (advance or choose)
    if (dialogue_is_active()) {
        if (dialogue_current_has_choices()) {
            for (int i = 1; i <= 9; i++) {
                if (input_choice_edge(i)) {
                    dialogue_select_choice_index(i - 1);
                    break;
                }
            }
        } else if (input_advance_dialogue_edge()) {
            dialogue_advance();
        }
    }

    if (g_mode == CONTROL_CAR) {
        car_update(&g_car, dt);
    } else {
        human_update(&g_human, dt);
    }

    // Update ball panning based on screen position relative to camera
    float bx = 0, by = 0;
    physics_get_position(g_ball_body, &bx, &by);
    // Map horizontal offset to pan in [-1,1] using viewport width and zoom
    float half_w = (float)g_w * 0.5f / g_cam.zoom;
    float pan = 0.0f;                        // left negative, right positive
    pan = (bx - g_cam.x - half_w) / half_w;  // center at camera middle
    if (pan < -1.0f)
        pan = -1.0f;
    if (pan > 1.0f)
        pan = 1.0f;
    g_ball_audio.pan = pan;

    // Update rear and front motor sounds; cutoff if camera too far
    float carx_, cary_;
    car_get_position(&g_car, &carx_, &cary_);
    float dx_ = carx_ - g_cam.x;
    float dy_ = cary_ - g_cam.y;
    bool too_far_ = (dx_ * dx_ + dy_ * dy_) > (1000.0f * 1000.0f);

    if (g_mode == CONTROL_CAR && !too_far_) {
        // Rear wheel
        float rear_w = car_get_rear_wheel_angular_speed(&g_car);
        float r_freq = 0.0f, r_gain = 0.0f;
        if (rear_w >= 0.5f) {
            float max_w = 100.0f;
            float wr = rear_w / max_w;
            if (wr > 1.0f)
                wr = 1.0f;
            r_freq = 0.0f + wr * 100.0f;
            r_gain = 0.10f + wr * 0.30f;
            g_car_rear_audio.playing = true;
        } else {
            g_car_rear_audio.playing = false;
        }
        g_car_rear_audio.u.osc.freq_hz = r_freq;
        g_car_rear_audio.gain = r_gain;

        // Front wheel
        float front_w = car_get_front_wheel_angular_speed(&g_car);
        float f_freq = 0.0f, f_gain = 0.0f;
        if (front_w >= 0.5f) {
            float max_wf = 100.0f;
            float wf = front_w / max_wf;
            if (wf > 1.0f)
                wf = 1.0f;
            f_freq = 0.0f + wf * 100.0f;
            f_gain = 0.08f + wf * 0.25f;
            g_car_front_audio.playing = true;
        } else {
            g_car_front_audio.playing = false;
        }
        g_car_front_audio.u.osc.freq_hz = f_freq;
        g_car_front_audio.gain = f_gain;
    } else {
        // Silence when not in car or too far
        g_car_rear_audio.playing = false;
        g_car_rear_audio.gain = 0.0f;
        g_car_rear_audio.u.osc.freq_hz = 0.0f;
        g_car_front_audio.playing = false;
        g_car_front_audio.gain = 0.0f;
        g_car_front_audio.u.osc.freq_hz = 0.0f;
    }

    // Sync active audio sources (music, car, gameplay one-shots and saws) in one batch
    AmeAudioSourceRef refs[128];
    int rc = 0;
    refs[rc++] = (AmeAudioSourceRef){&g_music, g_music_id};
    refs[rc++] = (AmeAudioSourceRef){&g_car_rear_audio, g_car_rear_audio_id};
    refs[rc++] = (AmeAudioSourceRef){&g_car_front_audio, g_car_front_audio_id};
    // Collect gameplay-managed sources (explosions, saws, etc.)
    rc += gameplay_collect_audio_refs(refs + rc, (int)(sizeof(refs) / sizeof(refs[0])) - rc,
                                      g_cam.x, g_cam.y, (float)g_w, g_cam.zoom, dt);
    ame_audio_sync_sources_refs(refs, (size_t)rc);

    // Update triggers (AABB overlap only)
    float hx, hy, cx, cy;
    human_get_position(&g_human, &hx, &hy);
    car_get_position(&g_car, &cx, &cy);
    triggers_update(hx, hy, g_human.w, g_human.h, cx, cy, g_car.cfg.body_w, g_car.cfg.body_h);

    // Gameplay update (audio panning, cleanup)
    gameplay_update(&g_human, &g_car, g_cam.x, g_cam.y, (float)g_w, g_cam.zoom, dt);

    // Smooth camera follow of active entity
    float tx, ty;
    if (g_mode == CONTROL_CAR) {
        car_get_position(&g_car, &tx, &ty);
    } else {
        human_get_position(&g_human, &tx, &ty);
    }
    ame_camera_set_target(&g_cam, tx, ty);
    ame_camera_update(&g_cam, dt);

    glClearColor(0.15f, 0.2f, 0.25f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    pipeline_begin(&g_cam, g_w, g_h);
    if (g_map_mesh.count > 0) {
        pipeline_mesh_submit(&g_map_mesh, 0, 0, 0, 1, 1, 1, 0.8f, 0.8f, 0.8f, 1.0f);
    }
    car_render(&g_car);
    human_render(&g_human);
    gameplay_render();

    // HUD and Dialogue UI
    ui_render_hud(&g_cam, g_w, g_h, &g_car, &g_human, &g_mode);
    ui_render_dialogue(&g_cam, g_w, g_h, dialogue_get_runtime(), dialogue_is_active());

    pipeline_end();
    SDL_GL_SwapWindow(g_window);
    return SDL_APP_CONTINUE;
}

void game_app_quit(void* appstate, int result) {
    (void)appstate;
    (void)result;
    atomic_store(&g_should_quit, true);
    if (g_logic_thread) {
        SDL_WaitThread(g_logic_thread, NULL);
        g_logic_thread = NULL;
    }
    car_shutdown(&g_car);
    human_shutdown(&g_human);
    pipeline_shutdown();
    free_obj_map(&g_map_mesh);
    gameplay_shutdown();
    dialogue_manager_shutdown();
    ui_shutdown();
    physics_shutdown();
    input_shutdown();
    ame_audio_shutdown();
    shutdown_gl();
}
