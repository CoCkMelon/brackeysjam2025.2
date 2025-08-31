#include "ui.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glad/gl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gameplay.h"
#include "path_util.h"
#include "render/pipeline.h"

static TTF_Font* g_font = NULL;
static int g_loaded_font_px = 0;
static char g_font_path[1024] = {0};

// Cached text textures to avoid recreating every frame
static GLuint g_hud_texture = 0;
static char g_last_hud_text[128] = {0};
static int g_hud_tw = 0, g_hud_th = 0;

static GLuint g_dialogue_texture = 0;
static char g_last_dialogue_text[2048] = {0};
static int g_dialogue_tw = 0, g_dialogue_th = 0;

static GLuint make_text_texture(const char* text, int wrap_w_pixels, int* out_w, int* out_h) {
    if (!g_font) {
        SDL_Log("make_text_texture: No font loaded");
        return 0;
    }
    if (!text || SDL_strlen(text) == 0) {
        SDL_Log("make_text_texture: No text provided");
        return 0;
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* surf =
        TTF_RenderText_Blended_Wrapped(g_font, text, (int)SDL_strlen(text), white, wrap_w_pixels);
    if (!surf) {
        SDL_Log("make_text_texture: TTF_RenderText_Blended_Wrapped failed: %s", SDL_GetError());
        return 0;
    }

    SDL_Surface* conv = surf;
    if (SDL_GetPixelFormatDetails(surf->format)->format != SDL_PIXELFORMAT_RGBA32) {
        conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surf);
        if (!conv) {
            SDL_Log("make_text_texture: SDL_ConvertSurface failed: %s", SDL_GetError());
            return 0;
        }
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 conv->pixels);

    if (out_w)
        *out_w = conv->w;
    if (out_h)
        *out_h = conv->h;
    SDL_DestroySurface(conv);

    return tex;
}

static void draw_tex_screen(const AmeCamera* cam,
                            int viewport_w,
                            int viewport_h,
                            GLuint tex,
                            int px_w,
                            int px_h,
                            float px_x,
                            float px_y) {
    if (!tex || !cam)
        return;
    // Convert top-left pixel coordinates to world coordinates using camera
    float wx = cam->x + px_x / cam->zoom;
    float wy = cam->y + px_y / cam->zoom;
    (void)viewport_w;
    (void)viewport_h;
    pipeline_sprite_quad_rot(wx, wy, (float)px_w, (float)px_h, 0.0f, tex, 1, 1, 1, 1);
}

static void ensure_font_for_viewport(int viewport_h) {
    // Responsive font sizing relative to a 720p reference
    float scale = (float)viewport_h / 720.0f;
    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 2.0f)
        scale = 2.5f;
    int desired_px = (int)(18.0f * scale);
    if (desired_px < 12)
        desired_px = 12;
    if (desired_px > 48)
        desired_px = 48;
    if (g_font && g_loaded_font_px == desired_px)
        return;
    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    if (g_font_path[0]) {
        g_font = TTF_OpenFont(g_font_path, desired_px);
        if (!g_font) {
            SDL_Log("TTF_OpenFont(%s,%d) failed: %s", g_font_path, desired_px, SDL_GetError());
        } else {
            g_loaded_font_px = desired_px;
        }
    }
    if (!g_font) {
        g_font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", desired_px);
        if (g_font)
            g_loaded_font_px = desired_px;
    }
}

void ui_init(void) {
    // In SDL3_ttf, TTF_Init returns true on success, false on failure
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return;
    }
    SDL_Log("TTF_Init succeeded");
    // Locate PixelifySans in assets and cache path
    const char* base = pathutil_base();
    const char* candidates[6];
    int ci = 0;
    static char p0[1024];
    static char p1[1024];
    if (base && base[0]) {
        SDL_snprintf(p0, sizeof(p0), "%sassets/PixelifySans-VariableFont_wght.ttf", base);
        candidates[ci++] = p0;
        SDL_snprintf(p1, sizeof(p1), "%s../assets/PixelifySans-VariableFont_wght.ttf", base);
        candidates[ci++] = p1;
    }
    candidates[ci++] = "assets/PixelifySans-VariableFont_wght.ttf";
    candidates[ci++] = "../assets/PixelifySans-VariableFont_wght.ttf";
    candidates[ci++] = "./assets/PixelifySans-VariableFont_wght.ttf";
    candidates[ci++] = NULL;
    for (int i = 0; i < ci && candidates[i]; ++i) {
        TTF_Font* test = TTF_OpenFont(candidates[i], 18);
        if (test) {
            SDL_strlcpy(g_font_path, candidates[i], sizeof(g_font_path));
            TTF_CloseFont(test);
            break;
        }
    }
    if (g_font_path[0]) {
        g_font = TTF_OpenFont(g_font_path, 18);
        if (!g_font) {
            SDL_Log("Failed to open PixelifySans at %s: %s", g_font_path, SDL_GetError());
        } else {
            g_loaded_font_px = 18;
            SDL_Log("Loaded UI font: %s", g_font_path);
        }
    }
    if (!g_font) {
        SDL_Log("Trying fallback font: /usr/share/fonts/TTF/DejaVuSans.ttf");
        g_font = TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf", 16);
        if (!g_font) {
            SDL_Log("TTF_OpenFont fallback failed: %s", SDL_GetError());
            // Try another common font path
            g_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 16);
            if (!g_font) {
                SDL_Log("Second fallback failed: %s", SDL_GetError());
            } else {
                g_loaded_font_px = 16;
                SDL_Log("Loaded fallback font: DejaVu (truetype path)");
            }
        } else {
            g_loaded_font_px = 16;
            SDL_Log("Loaded fallback font: DejaVu (TTF path)");
        }
    }
}

void ui_shutdown(void) {
    if (g_font) {
        TTF_CloseFont(g_font);
        g_font = NULL;
    }
    if (g_hud_texture) {
        glDeleteTextures(1, &g_hud_texture);
        g_hud_texture = 0;
    }
    if (g_dialogue_texture) {
        glDeleteTextures(1, &g_dialogue_texture);
        g_dialogue_texture = 0;
    }
    g_loaded_font_px = 0;
    g_font_path[0] = '\0';
    g_last_hud_text[0] = '\0';
    g_last_dialogue_text[0] = '\0';
    TTF_Quit();
}

void ui_render_hud(const AmeCamera* cam,
                   int viewport_w,
                   int viewport_h,
                   const Car* car,
                   const Human* human,
                   const ControlMode* cmode) {
    if (!car)
        return;
    ensure_font_for_viewport(viewport_h);

    // Text-only HUD at top-left with responsive margin
    float scale = (float)viewport_h / 720.0f;
    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 2.0f)
        scale = 2.5f;
    const float margin = 10.0f * scale;

    char hud[128];
    if (*cmode == CONTROL_CAR) {
        SDL_snprintf(hud, sizeof(hud), "HP: %.0f/%.0f    Fuel: %.0f/%.0f", car->hp, car->max_hp,
                     car->fuel, car->max_fuel);
    } else {
        SDL_snprintf(hud, sizeof(hud), "HP: %.0f/%.0f    Fuel: %.0f/%.0f", human->health.hp,
                     human->health.max_hp, car->fuel, car->max_fuel);
    }

    // Only recreate texture if text changed
    if (SDL_strcmp(hud, g_last_hud_text) != 0) {
        if (g_hud_texture) {
            glDeleteTextures(1, &g_hud_texture);
            g_hud_texture = 0;
        }
        g_hud_texture =
            make_text_texture(hud, viewport_w - (int)(2 * margin), &g_hud_tw, &g_hud_th);
        SDL_strlcpy(g_last_hud_text, hud, sizeof(g_last_hud_text));
    }

    if (g_hud_texture && g_hud_texture != 0) {
        float x = viewport_w / 2;
        float y = (float)viewport_h - margin - (float)g_hud_th;
        // Use text texture as sprite texture - pass the actual texture ID
        draw_tex_screen(cam, viewport_w, viewport_h, g_hud_texture, g_hud_tw, g_hud_th, x, y);
    }
}

void ui_render_dialogue(const AmeCamera* cam,
                        int viewport_w,
                        int viewport_h,
                        const AmeDialogueRuntime* rt,
                        bool active) {
    if (!active || !rt)
        return;
    ensure_font_for_viewport(viewport_h);

    float scale = (float)viewport_h / 720.0f;
    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 2.0f)
        scale = 2.5f;
    float margin = 25.0f * scale;
    int wrap_w = (int)((float)viewport_w * 0.9f);  // wrap to 90% of width

    const AmeDialogueLine* ln = NULL;
    if (rt->scene && rt->current_index < rt->scene->line_count) {
        ln = &rt->scene->lines[rt->current_index];
    }
    char buf[2048];
    buf[0] = '\0';
    if (ln) {
        if (ln->speaker && ln->speaker[0]) {
            SDL_strlcat(buf, ln->speaker, sizeof(buf));
            SDL_strlcat(buf, ": ", sizeof(buf));
        }
        if (ln->text)
            SDL_strlcat(buf, ln->text, sizeof(buf));
        if (ln->option_count > 0) {
            SDL_strlcat(buf, "\n\n", sizeof(buf));
            for (size_t i = 0; i < ln->option_count && i < 9; i++) {
                char line[256];
                SDL_snprintf(line, sizeof(line), "%zu) %s\n", i + 1,
                             ln->options[i].choice ? ln->options[i].choice : "");
                SDL_strlcat(buf, line, sizeof(buf));
            }
        }
    }

    // Only recreate texture if text changed
    if (SDL_strcmp(buf, g_last_dialogue_text) != 0) {
        if (g_dialogue_texture) {
            glDeleteTextures(1, &g_dialogue_texture);
            g_dialogue_texture = 0;
        }
        g_dialogue_texture = make_text_texture(buf, wrap_w, &g_dialogue_tw, &g_dialogue_th);
        SDL_strlcpy(g_last_dialogue_text, buf, sizeof(g_last_dialogue_text));
    }

    if (g_dialogue_texture && g_dialogue_texture != 0) {
        // Position dialogue at bottom-left of screen
        float x = margin;
        float y = margin;
        // Use text texture as sprite texture - no background bars
        draw_tex_screen(cam, viewport_w, viewport_h, g_dialogue_texture, g_dialogue_tw,
                        g_dialogue_th, viewport_w / 2.0f, y + viewport_h / 10.0f);
    }
}
