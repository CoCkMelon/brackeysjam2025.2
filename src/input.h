#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

bool input_init(void);
void input_shutdown(void);
void input_update(void);

int input_move_dir(void); // -1,0,1 (A/D or Left/Right)
int input_yaw_dir(void);  // -1,0,1 (Q/E or J/L) optional for car spin
bool input_jump_edge(void); // rising edge since last update
bool input_jump_down(void);
bool input_boost_down(void);   // Shift
bool input_pressed_switch(void); // E or Insert

// Dialogue helpers
bool input_advance_dialogue_edge(void); // Space or Enter edge
bool input_choice_edge(int index_1_to_9); // Number keys 1..9 edge

#ifdef __cplusplus
}
#endif

