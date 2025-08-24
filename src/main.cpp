#include "unitylike/Scene.h"
#include "input_local.h"

extern "C" {
#include "ame/ecs.h"
#include "ame/render_pipeline_ecs.h"
}

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glad/gl.h>
using namespace unitylike;
#include "GameManager.h"

// Globals for SDL callback
static SDL_Window* window = nullptr;
static SDL_GLContext glContext = nullptr;
static AmeEcsWorld* ameWorld = nullptr;
static Scene* scene = nullptr;
static bool inputInitialized = false;
static bool running = true;
static int windowWidth = 1280;
static int windowHeight = 720;
static CarGameManager* gameManager = nullptr;

// Fixed timestep
static const float fixedTimeStep = 1.0f / 60.0f;
static float accumulator = 0.0f;
static Uint64 lastTime = 0;

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    SDL_SetAppMetadata("Unity-like Box2D Car", "1.0", "com.example.unitylike-box2d-car");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow("AME - unitylike_box2d_car", windowWidth, windowHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) { SDL_Log("CreateWindow failed: %s", SDL_GetError()); return SDL_APP_FAILURE; }
    glContext = SDL_GL_CreateContext(window);
    if (!glContext) { SDL_Log("CreateContext failed: %s", SDL_GetError()); return SDL_APP_FAILURE; }
    if (!SDL_GL_MakeCurrent(window, glContext)) { SDL_Log("SDL_GL_MakeCurrent failed: %s", SDL_GetError()); return SDL_APP_FAILURE; }
    if (!SDL_GL_SetSwapInterval(1)) { SDL_GL_SetSwapInterval(0); }

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) { SDL_Log("gladLoadGL failed"); return SDL_APP_FAILURE; }

    // ECS + scene
    ameWorld = ame_ecs_world_create();
    ecs_world_t* world = (ecs_world_t*)ame_ecs_world_ptr(ameWorld);
    scene = new Scene(world);

    // Input
    inputInitialized = input_init();

    // Create a single GameManager root that will build the scene via behaviours
    GameObject root = scene->Create("GameManager");
    gameManager = &root.AddScript<CarGameManager>();
    gameManager->screenWidth = windowWidth;
    gameManager->screenHeight = windowHeight;
    // Note: actual scene construction is in behaviours; main.cpp only sets up engine loop.

    int drawableW = 0, drawableH = 0;
    SDL_GetWindowSize(window, &drawableW, &drawableH);
    if (drawableW <= 0 || drawableH <= 0) { drawableW = windowWidth; drawableH = windowHeight; }
    glViewport(0, 0, drawableW, drawableH);

    lastTime = SDL_GetPerformanceCounter();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        running = false;
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        if (event->window.windowID == SDL_GetWindowID(window)) {
            windowWidth = event->window.data1;
            windowHeight = event->window.data2;
            int drawableW = 0, drawableH = 0;
            SDL_GetWindowSize(window, &drawableW, &drawableH);
            if (drawableW <= 0 || drawableH <= 0) { drawableW = windowWidth; drawableH = windowHeight; }
            glViewport(0, 0, drawableW, drawableH);
            // Update camera viewport
            if (gameManager) {
                gameManager->SetViewport(windowWidth, windowHeight);
            }
        }
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    if (!running) return SDL_APP_SUCCESS;

    Uint64 currentTime = SDL_GetPerformanceCounter();
    float dt = (float)(currentTime - lastTime) / SDL_GetPerformanceFrequency();
    lastTime = currentTime;
    dt = SDL_min(dt, 0.25f);

    if (inputInitialized) {
        input_begin_frame();
        // Check for ESC quit signal
        if (input_should_quit()) {
            running = false;
            return SDL_APP_SUCCESS;
        }
    }

    accumulator += dt;
    while (accumulator >= fixedTimeStep) {
        scene->StepFixed(fixedTimeStep);
        accumulator -= fixedTimeStep;
    }

    scene->Step(dt);

    glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_GL_MakeCurrent(window, glContext);
    ecs_world_t* world = (ecs_world_t*)ame_ecs_world_ptr(ameWorld);
    ame_rp_run_ecs(world);

    glFlush();
    SDL_GL_SwapWindow(window);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (inputInitialized) input_shutdown();
    if (ameWorld) { ame_ecs_world_destroy(ameWorld); ameWorld = nullptr; }
    if (scene) { delete scene; scene = nullptr; }
    if (glContext) { SDL_GL_DestroyContext(glContext); glContext = nullptr; }
    if (window) { SDL_DestroyWindow(window); window = nullptr; }
    SDL_Quit();
}

