#pragma once
// Central app entry points to keep main.c minimal
#ifdef __cplusplus
extern "C" {
#endif

int game_app_init(void);
int game_app_event(void *appstate, void *sdl_event);
int game_app_iterate(void *appstate);
void game_app_quit(void *appstate, int result);

#ifdef __cplusplus
}
#endif

