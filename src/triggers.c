#include "triggers.h"
#include <SDL3/SDL.h>
#include <string.h>

#define MAX_TRIGGERS 128
static Trigger g_triggers[MAX_TRIGGERS];
static int g_count = 0;

static inline int aabb_overlap(Aabb a, Aabb b) {
    float ax0 = a.x - a.w * 0.5f, ay0 = a.y - a.h * 0.5f, ax1 = a.x + a.w * 0.5f,
          ay1 = a.y + a.h * 0.5f;
    float bx0 = b.x - b.w * 0.5f, by0 = b.y - b.h * 0.5f, bx1 = b.x + b.w * 0.5f,
          by1 = b.y + b.h * 0.5f;
    return !(ax1 < bx0 || bx1 < ax0 || ay1 < by0 || by1 < ay0);
}

void triggers_init(void) {
    g_count = 0;
    memset(g_triggers, 0, sizeof(g_triggers));
}
void triggers_clear(void) {
    g_count = 0;
}

void triggers_add(const char* name, Aabb box, int once, TriggerCallback cb, void* user) {
    if (g_count >= MAX_TRIGGERS)
        return;
    g_triggers[g_count].name = name;
    g_triggers[g_count].box = box;
    g_triggers[g_count].once = once ? 1 : 0;
    g_triggers[g_count].fired = 0;
    g_triggers[g_count].cb = cb;
    g_triggers[g_count].user = user;
    g_count++;
}

void triggers_update(float player_x,
                     float player_y,
                     float player_w,
                     float player_h,
                     float car_x,
                     float car_y,
                     float car_w,
                     float car_h) {
    Aabb pa = {player_x, player_y, player_w, player_h};
    Aabb ca = {car_x, car_y, car_w, car_h};
    for (int i = 0; i < g_count; i++) {
        if (g_triggers[i].once && g_triggers[i].fired)
            continue;
        int hit = aabb_overlap(g_triggers[i].box, pa) || aabb_overlap(g_triggers[i].box, ca);
        if (hit) {
            if (g_triggers[i].cb) {
                g_triggers[i].cb(g_triggers[i].name, g_triggers[i].user);
            }
            if (g_triggers[i].once) {
                g_triggers[i].fired = 1;
            }
        }
    }
}

void triggers_fire(const char* name) {
    if (!name)
        return;
    for (int i = 0; i < g_count; i++) {
        if (g_triggers[i].name && SDL_strcmp(g_triggers[i].name, name) == 0) {
            if (g_triggers[i].once && g_triggers[i].fired)
                return;
            if (g_triggers[i].cb) {
                g_triggers[i].cb(g_triggers[i].name, g_triggers[i].user);
            }
            if (g_triggers[i].once) {
                g_triggers[i].fired = 1;
            }
            return;
        }
    }
}
