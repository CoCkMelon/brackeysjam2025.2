#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "app.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv) {
  (void)appstate; (void)argc; (void)argv;
  return game_app_init() ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  (void)appstate;
  int r = game_app_event(appstate, (void*)event);
  return r;
}

SDL_AppResult SDL_AppIterate(void *appstate) {
  int r = game_app_iterate(appstate);
  return r;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  game_app_quit(appstate, (int)result);
}

