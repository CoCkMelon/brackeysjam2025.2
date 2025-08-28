#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // Car abilities
  bool car_jump;   // allow car hop on Space
  bool car_boost;  // allow boost multiplier on Shift
  bool car_fly;    // allow helicopter-like sustained lift while Space is held
  // Extend with player abilities if needed
} AbilityState;

// Global ability state (read/write directly)
extern AbilityState g_abilities;

// Basic lifecycle helpers
void abilities_init(void);
void abilities_reset(void);

#ifdef __cplusplus
}
#endif

