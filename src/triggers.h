#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float x,y,w,h; } Aabb;

typedef void (*TriggerCallback)(const char* name, void* user);

// Fire a trigger by name manually (e.g., from dialogue trigger callback)
void triggers_fire(const char* name);

typedef struct Trigger {
  const char* name;
  Aabb box;
  int once;         // if true, disable after firing
  int fired;        // internal state
  TriggerCallback cb;
  void* user;
} Trigger;

void triggers_init(void);
void triggers_clear(void);

// Add a box trigger with callback
void triggers_add(const char* name, Aabb box, int once, TriggerCallback cb, void* user);

// Update with world-space positions (Y-up) and simple extents for player/car
void triggers_update(float player_x, float player_y, float player_w, float player_h,
                     float car_x, float car_y, float car_w, float car_h);

#ifdef __cplusplus
}
#endif

