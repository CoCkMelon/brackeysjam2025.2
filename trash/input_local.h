#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

// Project-local asyncinput wrapper
bool input_init(void);
void input_shutdown(void);
void input_begin_frame(void);

// Queries
bool input_should_quit(void);
int  input_move_dir(void);      // -1,0,1
int  input_yaw_dir(void);      // -1,0,1
bool input_jump_edge(void);     // just-pressed this frame

#ifdef __cplusplus
}
#endif

