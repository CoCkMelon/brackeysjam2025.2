#include "dialogue_manager.h"
#include <SDL3/SDL.h>
#include <string.h>
#include "dialogue_generated.h"
#include "triggers.h"

// Internal runtime and state
static AmeDialogueRuntime g_rt;
static bool g_active = false;

static const AmeDialogueScene* load_local_dialogue(const char* name) {
    if (!name)
        return NULL;
    for (size_t i = 0; i < ame__generated_scenes_count; i++) {
        const AmeDialogueScene* scene = ame__generated_scenes[i];
        if (scene && scene->scene && SDL_strcmp(scene->scene, name) == 0) {
            return scene;
        }
    }
    return NULL;
}

static void dialogue_trigger_hook(const char* trigger_name,
                                  const AmeDialogueLine* line,
                                  void* user) {
    (void)line;
    (void)user;
    if (trigger_name && trigger_name[0]) {
        // Forward to our trigger system
        triggers_fire(trigger_name);
    }
}

bool dialogue_manager_init(void) {
    memset(&g_rt, 0, sizeof(g_rt));
    g_active = false;
    return true;
}

void dialogue_manager_shutdown(void) {
    // Currently no heap resources owned here that require explicit release.
    g_active = false;
    memset(&g_rt, 0, sizeof(g_rt));
}

bool dialogue_start_scene(const char* scene_name) {
    const AmeDialogueScene* sc = load_local_dialogue(scene_name);
    if (!sc) {
        SDL_Log("Dialogue: scene not found: %s", scene_name ? scene_name : "<null>");
        return false;
    }
    if (!ame_dialogue_runtime_init(&g_rt, sc, dialogue_trigger_hook, NULL)) {
        SDL_Log("Dialogue: failed to init runtime for scene: %s", sc->scene);
        return false;
    }
    g_active = true;
    ame_dialogue_play_current(&g_rt);
    return true;
}

bool dialogue_is_active(void) {
    return g_active;
}
const AmeDialogueRuntime* dialogue_get_runtime(void) {
    return g_active ? &g_rt : NULL;
}

bool dialogue_current_has_choices(void) {
    return g_active && ame_dialogue_current_has_choices(&g_rt);
}

const AmeDialogueLine* dialogue_select_choice_index(int idx) {
    if (!g_active)
        return NULL;
    const AmeDialogueLine* cur = (g_rt.scene && g_rt.current_index < g_rt.scene->line_count)
                                     ? &g_rt.scene->lines[g_rt.current_index]
                                     : NULL;
    if (!cur || idx < 0 || idx >= (int)cur->option_count)
        return NULL;
    return ame_dialogue_select_choice(&g_rt, cur->options[idx].next);
}

const AmeDialogueLine* dialogue_advance(void) {
    if (!g_active)
        return NULL;
    return ame_dialogue_advance(&g_rt);
}
