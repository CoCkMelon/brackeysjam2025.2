#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

bool input_init(void);
void input_shutdown(void);
void input_update(void);

// Movement APIs
int input_move_dir(void);   // -1,0,1 (Left/Right or A/D) for human movement
int input_accel_dir(void);  // -1,0,1 (S/W) for car acceleration/backwards/forwards
int input_yaw_dir(void);    // -1,0,1 (A/D) for car yaw/spin

bool input_jump_edge(void);  // rising edge since last update
bool input_jump_down(void);
bool input_boost_down(void);      // Shift
bool input_pressed_switch(void);  // E or Insert
bool input_quit_requested(void);  // ESC

// Dialogue helpers
bool input_advance_dialogue_edge(void);    // Space or Enter edge
bool input_choice_edge(int index_1_to_9);  // Number keys 1..9 edge

#ifdef __cplusplus
}
#endif
