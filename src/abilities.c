#include "abilities.h"
#include <stdatomic.h>
#include <string.h>

typedef struct {
    atomic_bool car_jump;
    atomic_bool car_boost;
    atomic_bool car_fly;
} AbilityStateAtomic;

static AbilityStateAtomic g_abilities_atomic;  // zero-initialized

void abilities_init(void) {
    abilities_reset();
}

void abilities_reset(void) {
    atomic_store(&g_abilities_atomic.car_jump, false);
    atomic_store(&g_abilities_atomic.car_boost, false);
    atomic_store(&g_abilities_atomic.car_fly, false);
}

bool ability_get_car_jump(void) {
    return atomic_load(&g_abilities_atomic.car_jump);
}
void ability_set_car_jump(bool v) {
    atomic_store(&g_abilities_atomic.car_jump, v);
}

bool ability_get_car_boost(void) {
    return atomic_load(&g_abilities_atomic.car_boost);
}
void ability_set_car_boost(bool v) {
    atomic_store(&g_abilities_atomic.car_boost, v);
}

bool ability_get_car_fly(void) {
    return atomic_load(&g_abilities_atomic.car_fly);
}
void ability_set_car_fly(bool v) {
    atomic_store(&g_abilities_atomic.car_fly, v);
}
