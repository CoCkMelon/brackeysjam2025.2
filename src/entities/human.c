#include "human.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <string.h>
#include "../input.h"
#include "../physics.h"
#include "../render/pipeline.h"

static GLuint upload_subtexture_rgba8(const unsigned char* pixels, int w, int h, int stride_bytes) {
    // Make a tightly packed buffer if stride != w*4
    unsigned char* tmp = NULL;
    const unsigned char* src = pixels;
    size_t row_bytes = (size_t)w * 4;
    if (stride_bytes != (int)row_bytes) {
        tmp = (unsigned char*)SDL_malloc((size_t)h * row_bytes);
        for (int y = 0; y < h; y++) {
            memcpy(tmp + y * row_bytes, pixels + y * stride_bytes, row_bytes);
        }
        src = tmp;
    }
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (tmp)
        SDL_free(tmp);
    return t;
}

static int load_player_frames(GLuint* out_frames,
                              int max_frames,
                              int* out_w,
                              int* out_h,
                              const HumanAnimConfig* cfg) {
    // Load the combined spritesheet defined in the centralized config
    const char* candidates[] = {HUMAN_SPRITESHEET_PATH, NULL};
    SDL_Surface* surf = NULL;
    for (int i = 0; candidates[i]; ++i) {
        surf = IMG_Load(candidates[i]);
        if (surf)
            break;
    }
    if (!surf) {
        SDL_Log("Failed to load player spritesheet: %s", HUMAN_SPRITESHEET_PATH);
        return 0;
    }
    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!conv) {
        SDL_Log("ConvertSurface failed: %s", SDL_GetError());
        return 0;
    }
    int tile_w = cfg ? cfg->tile_w : HUMAN_TILE_W;
    int tile_h = cfg ? cfg->tile_h : HUMAN_TILE_H;
    int row = cfg ? cfg->row : HUMAN_SPRITESHEET_ROW;
    if (tile_w <= 0)
        tile_w = HUMAN_TILE_W;
    if (tile_h <= 0)
        tile_h = HUMAN_TILE_H;
    if (row < 0)
        row = HUMAN_SPRITESHEET_ROW;
    int cols = conv->w / tile_w;
    int frames = cols < max_frames ? cols : max_frames;
    for (int i = 0; i < frames; i++) {
        unsigned char* base = (unsigned char*)conv->pixels;
        int sx = i * tile_w;
        int sy = row * tile_h;
        if (sy + tile_h > conv->h)
            break;
        unsigned char* start = base + sy * conv->pitch + sx * 4;
        unsigned char* tmp = (unsigned char*)SDL_malloc((size_t)tile_h * (size_t)tile_w * 4);
        for (int y = 0; y < tile_h; y++) {
            memcpy(tmp + (size_t)y * tile_w * 4, start + (size_t)y * conv->pitch,
                   (size_t)tile_w * 4);
        }
        out_frames[i] = upload_subtexture_rgba8(tmp, tile_w, tile_h, tile_w * 4);
        SDL_free(tmp);
    }
    if (out_w)
        *out_w = tile_w;
    if (out_h)
        *out_h = tile_h;
    SDL_DestroySurface(conv);
    return frames;
}

void human_init(Human* h) {
    if (!h)
        return;
    // Default animation controls centralized in human.h
    h->cfg = human_default_anim_config();

    h->hidden = 0;
    h->frame_count = 0;
    h->current_frame = h->cfg.idle;
    h->anim_time = 0.0f;
    h->facing = 1;  // face right initially
    h->was_grounded = true;
    h->jump_anim_playing = 0;
    h->jump_anim_time = 0.0f;

    int fw = HUMAN_TILE_W, fh = HUMAN_TILE_H;
    h->frame_count = load_player_frames(h->frames, 32, &fw, &fh, &h->cfg);
    if (h->frame_count <= 0) {
        unsigned char px[16 * 16 * 4];
        memset(px, 255, sizeof(px));
        h->frames[0] = upload_subtexture_rgba8(px, 16, 16, 16 * 4);
        h->frame_count = 1;
        fw = 16;
        fh = 16;
    }
    h->w = (float)fw;
    h->h = (float)fh;
    h->body = physics_create_dynamic_box(120, 120, h->w, h->h, 1.0f, 0.4f);
    h->x_control_lock = 0.0f;
    // Health defaults
    h->health.max_hp = 100.0f;
    h->health.hp = h->health.max_hp;
    // Add a small foot sensor slightly below the body to detect ground and walls via contacts
    if (h->body) {
        float sensor_w = h->w * 0.8f;
        float sensor_h = 2.0f;
        float offset_x = 0.0f;
        float offset_y = -h->h * 0.5f - 1.0f;
        physics_add_sensor_box(h->body, sensor_w, sensor_h, offset_x, offset_y);
    }
}

void human_shutdown(Human* h) {
    (void)h;
}

void human_fixed(Human* h, float dt) {
    if (!h || !h->body)
        return;
    if (h->pending_teleport) {
        physics_teleport_body(h->body, h->pending_tx, h->pending_ty);
        h->pending_teleport = 0;
    }
    int dir = input_move_dir();
    float target_vx = 50.0f * (float)dir;
    bool grounded = physics_is_grounded(h->body);

    // Update facing when input present
    if (dir > 0)
        h->facing = 1;
    else if (dir < 0)
        h->facing = -1;

    // Decrease temporary horizontal control lock (after wall-jump)
    if (h->x_control_lock > 0.0f) {
        h->x_control_lock -= dt;
        if (h->x_control_lock < 0.0f)
            h->x_control_lock = 0.0f;
    }
    // Apply desired horizontal velocity only if not in control lock
    if (h->x_control_lock <= 0.0f) {
        physics_set_velocity_x(h->body, target_vx);
    }

    // Animation selection using centralized config
    if (h->jump_anim_playing) {
        // If playing takeoff, advance through exactly 2 frames at jump_fps then hold first jump
        // frame
        h->jump_anim_time += dt;
        int frame_i = (int)(h->jump_anim_time * h->cfg.jump_fps);  // 0,1,2,...
        if (frame_i <= 0)
            frame_i = 0;
        if (frame_i >= 2) {
            // Finished the two-frame takeoff
            h->jump_anim_playing = 0;
            frame_i = 0;  // fall-through to holding first frame below
        }
        int idx = h->cfg.jump_first + (frame_i > 1 ? 1 : frame_i);
        if (idx >= 0 && idx < h->frame_count)
            h->current_frame = idx;
        else
            h->current_frame = h->cfg.idle;
    } else if (!grounded) {
        // Hold first jump frame while airborne
        h->current_frame = (h->cfg.jump_first < h->frame_count ? h->cfg.jump_first : h->cfg.idle);
    } else if (dir != 0 && h->cfg.walk_count > 0) {
        // If just landed, restart walk cycle and stop any residual jump takeoff
        if (!h->was_grounded) {
            h->anim_time = 0.0f;
            h->jump_anim_playing = 0;
            h->jump_anim_time = 0.0f;
        }
        h->anim_time += dt;
        int cycle = (int)(h->anim_time * h->cfg.walk_fps) % h->cfg.walk_count;
        int idx = h->cfg.walk[cycle];
        if (idx >= 0 && idx < h->frame_count)
            h->current_frame = idx;
        else
            h->current_frame = h->cfg.idle;
    } else {
        // Idle when grounded and no input; stop any residual jump takeoff
        h->current_frame = (h->cfg.idle < h->frame_count ? h->cfg.idle : 0);
        h->anim_time = 0.0f;
        h->jump_anim_playing = 0;
        h->jump_anim_time = 0.0f;
    }
    if (input_jump_edge()) {
        // Start one-shot 2-frame jump takeoff animation immediately
        h->jump_anim_playing = 1;
        h->jump_anim_time = 0.0f;
        h->current_frame = (h->cfg.jump_first < h->frame_count ? h->cfg.jump_first : h->cfg.idle);
        if (grounded) {
            physics_apply_impulse(h->body, 0.0f, 50000.0f);
            grounded = false;  // treat as airborne now for anim purposes
        } else {
            int wall_dir = 0;
            if (physics_is_touching_wall(h->body, &wall_dir) && wall_dir != 0) {
                // Wall jump: set horizontal velocity away from wall and add vertical impulse
                float vx, vy;
                physics_get_velocity(h->body, &vx, &vy);
                vx = -120.0f * (float)wall_dir + target_vx / 2;  // push away from wall
                physics_set_velocity(h->body, vx, vy);
                float iy = 50000.0f;
                physics_apply_impulse(h->body, 0.0f, iy);
                h->x_control_lock = 0.3f;  // ~300ms to preserve horizontal kick
            } else {
                SDL_Log("jump not grounded");
            }
        }
    }
    h->was_grounded = grounded;
}

void human_update(Human* h, float dt) {
    (void)dt;
    (void)h;
    float x, y;
    human_get_position(h, &x, &y);
    if (y < -10000)
        h->health.hp = 0;
}

void human_render(const Human* h) {
    if (!h || h->hidden)
        return;
    float x = 0, y = 0;
    physics_get_position(h->body, &x, &y);
    GLuint tex = h->frames[h->current_frame % (h->frame_count > 0 ? h->frame_count : 1)];
    // Human currently unrotated; pass 0 angle but use rotated sprite API
    pipeline_sprite_quad_rot(x, y, h->w * (float)h->facing, h->h, 0.0f, tex, 1, 1, 1, 1);
}

void human_set_position(Human* h, float x, float y) {
    if (!h)
        return;
    h->pending_teleport = 1;
    h->pending_tx = x;
    h->pending_ty = y;
}
void human_get_position(const Human* h, float* x, float* y) {
    if (!h)
        return;
    physics_get_position(h->body, x, y);
}
void human_hide(Human* h, bool hide) {
    if (!h)
        return;
    h->hidden = hide ? 1 : 0;
    // Also disable physics body so hidden human doesn't collide or receive forces
    physics_set_body_enabled(h->body, !hide);
}

void human_apply_damage(Human* h, float dmg) {
    if (!h)
        return;
    h->health.hp -= dmg;
    if (h->health.hp < 0.0f)
        h->health.hp = 0.0f;
    SDL_Log("Human HP: %.1f / %.1f", h->health.hp, h->health.max_hp);
    if (h->health.hp == 0.0f) {
        SDL_Log("Human defeated");
    }
}
