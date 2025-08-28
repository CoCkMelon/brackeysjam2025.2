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
#include "entities/car.h"
#include "entities/human.h"
#include "input.h"
#include "obj_map.h"
#include "path_util.h"
#include "physics.h"
#include "render/pipeline.h"
#include "triggers.h"

static SDL_Window* g_window = NULL;
static SDL_GLContext g_gl = NULL;
static int g_w = 1280, g_h = 720;

static AmeCamera g_cam;
static atomic_bool g_should_quit = false;

// Dialogue runtime (engine API)
static AmeDialogueRuntime g_dlg_rt;
static const AmeDialogueScene* g_dlg_scene = NULL;
static bool g_dlg_active = false;

typedef enum { CONTROL_HUMAN, CONTROL_CAR } ControlMode;
static ControlMode g_mode = CONTROL_HUMAN;

static Human g_human;
static Car g_car;

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
static const float kFixedDt = 0.001f;  // 1000 Hz

// #define MAP_OBJ_NAME "test dimensions.obj"
#define MAP_OBJ_NAME "car_village.obj"

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
        SDL_CreateWindow("Sidescroller Racer", g_w, g_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

// Dialogue trigger hook: forward to our trigger manager by name
static void dialogue_trigger_hook(const char* trigger_name,
                                  const AmeDialogueLine* line,
                                  void* user) {
    (void)line;
    (void)user;
    if (trigger_name && trigger_name[0]) {
        triggers_fire(trigger_name);
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
        human_get_position(&g_human, &hx, &hy);
        car_get_position(&g_car, &cx, &cy);
        float dx = hx - cx, dy = hy - cy;
        float d2 = dx * dx + dy * dy;
        SDL_Log("Switch pressed: distance=%.2f, threshold=%.2f", sqrtf(d2), 32.0f);
        if (d2 < 32.0f * 32.0f || g_mode == CONTROL_CAR) {
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
        g_abilities.car_jump = true;
        SDL_Log("Ability unlocked: car_jump");
    } else if (SDL_strcmp(name, "unlock_car_boost") == 0) {
        g_abilities.car_boost = true;
        SDL_Log("Ability unlocked: car_boost");
    } else if (SDL_strcmp(name, "unlock_car_fly") == 0) {
        g_abilities.car_fly = true;
        SDL_Log("Ability unlocked: car_fly");
    }
}

int game_app_init(void) {
    if (!SDL_Init(SDL_INIT_VIDEO))
        return 0;
    if (!init_gl())
        return 0;
    ame_camera_init(&g_cam);
    g_cam.zoom = 3.0f;
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

    // Music (looping)
    if (!ame_audio_source_load_opus_file(
            &g_music, "assets/brackeys_platformer_assets/music/time_for_adventure.opus", true)) {
        SDL_Log("music load failed");
    }
    g_music.gain = 0.35f;
    g_music.pan = 0.0f;
    g_music.playing = true;

    // Car engine hums (non-spatial)
    ame_audio_source_init_sigmoid(&g_car_rear_audio, 80.0f, 4.0f, 0.20f);
    g_car_rear_audio.pan = 0.0f;
    // Front wheel motor
    ame_audio_source_init_sigmoid(&g_car_front_audio, 80.0f, 4.0f, 0.18f);
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

    // Init dialogue runtime from embedded scenes
    g_dlg_scene = ame_dialogue_load_embedded("sample");
    if (!g_dlg_scene)
        g_dlg_scene = ame_dialogue_load_embedded("museum_entrance");
    if (g_dlg_scene) {
        if (ame_dialogue_runtime_init(&g_dlg_rt, g_dlg_scene, dialogue_trigger_hook, NULL)) {
            g_dlg_active = true;
            const AmeDialogueLine* ln = ame_dialogue_play_current(&g_dlg_rt);
            if (ln && ln->text)
                SDL_Log("Dialogue: %s: %s", ln->speaker ? ln->speaker : "", ln->text);
        }
    }

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

    // If map loaded, spawn near its center; otherwise fallback
    if (g_map_mesh.count > 0 && g_map_mesh.pos) {
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (unsigned i = 0; i < g_map_mesh.count; i++) {
            float x = g_map_mesh.pos[i * 2 + 0];
            float y = g_map_mesh.pos[i * 2 + 1];
            if (x < minx)
                minx = x;
            if (x > maxx)
                maxx = x;
            if (y < miny)
                miny = y;
            if (y > maxy)
                maxy = y;
        }
        float cx = 0.5f * (minx + maxx);
        float cy = 0.5f * (miny + maxy);
        // Start above center and raise until no overlaps with world colliders
        float startY = cy + 40.0f;
        float carW = g_car.cfg.body_w, carH = g_car.cfg.body_h + g_car.cfg.wheel_radius * 2.0f;
        float humanW = g_human.w, humanH = g_human.h;
        float y = startY;
        for (int iter = 0; iter < 200; ++iter) {
            bool carHit = physics_overlap_aabb(cx, y, carW, carH, g_car.body, g_human.body);
            bool humanHit = physics_overlap_aabb(cx, y, humanW, humanH, g_car.body, g_human.body);
            if (!carHit && !humanHit)
                break;
            y += 5.0f;
        }
        car_set_position(&g_car, cx, y);
        human_set_position(&g_human, cx, y);
    } else {
        float x = 140.0f, y = 180.0f;
        for (int iter = 0; iter < 200; ++iter) {
            bool carHit = physics_overlap_aabb(x, y, g_car.cfg.body_w,
                                               g_car.cfg.body_h + g_car.cfg.wheel_radius * 2.0f,
                                               g_car.body, g_human.body);
            bool humanHit =
                physics_overlap_aabb(x, y, g_human.w, g_human.h, g_car.body, g_human.body);
            if (!carHit && !humanHit)
                break;
            y += 5.0f;
        }
        car_set_position(&g_car, x, y);
        human_set_position(&g_human, x, y);
    }
    SDL_Log("Spawned at: car=(%.1f,%.1f), human=(%.1f,%.1f)", (double)0, (double)0, (double)0,
            (double)0);  // markers in log

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
    // Keep human near car when driving to allow quick switch back
    if (g_mode == CONTROL_CAR) {
        float cx, cy;
        car_get_position(&g_car, &cx, &cy);
        human_set_position(&g_human, cx, cy);
    }

    // Dialogue input handling (advance or choose)
    if (g_dlg_active) {
        const AmeDialogueLine* ln = NULL;
        if (ame_dialogue_current_has_choices(&g_dlg_rt)) {
            for (int i = 1; i <= 9; i++) {
                if (input_choice_edge(i)) {
                    // Fetch current line to get option index safely
                    const AmeDialogueLine* cur =
                        (g_dlg_rt.scene && g_dlg_rt.current_index < g_dlg_rt.scene->line_count)
                            ? &g_dlg_rt.scene->lines[g_dlg_rt.current_index]
                            : NULL;
                    if (cur && i - 1 < (int)cur->option_count) {
                        ln = ame_dialogue_select_choice(&g_dlg_rt, cur->options[i - 1].next);
                    }
                    break;
                }
            }
        } else if (input_advance_dialogue_edge()) {
            ln = ame_dialogue_advance(&g_dlg_rt);
        }
        if (ln && ln->text) {
            SDL_Log("Dialogue: %s: %s", ln->speaker ? ln->speaker : "", ln->text);
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
            r_freq = 60.0f + wr * 140.0f;
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
            f_freq = 60.0f + wf * 140.0f;
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

    // Sync active audio sources
    AmeAudioSourceRef arefs[3] = {{&g_music, g_music_id},
                                  {&g_car_rear_audio, g_car_rear_audio_id},
                                  {&g_car_front_audio, g_car_front_audio_id}};
    ame_audio_sync_sources_refs(arefs, 3);

    // Update triggers (AABB overlap only)
    float hx, hy, cx, cy;
    human_get_position(&g_human, &hx, &hy);
    car_get_position(&g_car, &cx, &cy);
    triggers_update(hx, hy, g_human.w, g_human.h, cx, cy, g_car.cfg.body_w, g_car.cfg.body_h);

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
        pipeline_mesh_submit(&g_map_mesh, 0, 0, 1, 1, 0.8f, 0.8f, 0.8f, 1.0f);
    }
    car_render(&g_car);
    human_render(&g_human);
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
    physics_shutdown();
    input_shutdown();
    ame_audio_shutdown();
    shutdown_gl();
}
