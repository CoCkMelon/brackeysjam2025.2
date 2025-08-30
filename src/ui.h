#pragma once
#include <stdbool.h>
#include "ame/camera.h"
#include "entities/car.h"
#include "ame_dialogue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize UI subsystem (fonts, etc.)
void ui_init(void);
// Shutdown UI subsystem
void ui_shutdown(void);

// Render HUD (HP/Fuel) at top-left
void ui_render_hud(const AmeCamera* cam, int viewport_w, int viewport_h, const Car* car);

// Render dialogue UI (speaker, text, choices) at bottom-left when active
void ui_render_dialogue(const AmeCamera* cam,
                        int viewport_w,
                        int viewport_h,
                        const AmeDialogueRuntime* rt,
                        bool active);

#ifdef __cplusplus
}
#endif

