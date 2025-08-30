#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>
#include "ame_dialogue.h"

// Initialize/shutdown the dialogue manager
bool dialogue_manager_init(void);
void dialogue_manager_shutdown(void);

// Start a dialogue scene by name (must exist in generated dialogue scenes)
// Returns true on success.
bool dialogue_start_scene(const char* scene_name);

// Query active state and access runtime for UI
bool dialogue_is_active(void);
const AmeDialogueRuntime* dialogue_get_runtime(void);

// Input helpers mirroring existing usage
bool dialogue_current_has_choices(void);
// Select choice by 0-based index; returns next line or NULL
const AmeDialogueLine* dialogue_select_choice_index(int idx);
// Advance to next line; returns next line or NULL
const AmeDialogueLine* dialogue_advance(void);

#ifdef __cplusplus
}
#endif
