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

// Global ability state is now managed atomically via accessors.
// Direct access to a global struct has been removed to ensure thread-safety.

// Basic lifecycle helpers
void abilities_init(void);
void abilities_reset(void);

// Atomic accessors for car abilities
bool ability_get_car_jump(void);
void ability_set_car_jump(bool v);

bool ability_get_car_boost(void);
void ability_set_car_boost(bool v);

bool ability_get_car_fly(void);
void ability_set_car_fly(bool v);

#ifdef __cplusplus
}
#endif
