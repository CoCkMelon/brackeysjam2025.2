#include "path_util.h"
#include <SDL3/SDL.h>
#include <string.h>

static char g_base[1024] = {0};
static int g_inited = 0;

void pathutil_init(void) {
    if (g_inited)
        return;
    g_inited = 1;
    const char* base = SDL_GetBasePath();
    if (base && base[0]) {
        SDL_strlcpy(g_base, base, sizeof(g_base));
    } else {
        g_base[0] = '\0';
    }
}

const char* pathutil_base(void) {
    if (!g_inited)
        pathutil_init();
    return g_base;
}
