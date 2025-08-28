#include "human.h"
#include "../physics.h"
#include "../input.h"
#include "../render/pipeline.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <stdlib.h>
#include <string.h>

static GLuint upload_subtexture_rgba8(const unsigned char* pixels, int w, int h, int stride_bytes){
  // Make a tightly packed buffer if stride != w*4
  unsigned char* tmp = NULL; const unsigned char* src = pixels; size_t row_bytes = (size_t)w * 4;
  if(stride_bytes != (int)row_bytes){ tmp = (unsigned char*)SDL_malloc((size_t)h * row_bytes); for(int y=0;y<h;y++){ memcpy(tmp + y*row_bytes, pixels + y*stride_bytes, row_bytes); } src = tmp; }
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,src);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  if(tmp) SDL_free(tmp);
  return t;
}

static int load_player_frames(GLuint* out_frames, int max_frames, int* out_w, int* out_h, const HumanAnimConfig* cfg){
  const char* candidates[] = {
    "assets/player.png",
    "examples/kenney_pixel-platformer/Tilemap/tilemap-characters.png",
    NULL
  };
  SDL_Surface* surf = NULL;
  for(int i=0;candidates[i];++i){ surf = IMG_Load(candidates[i]); if(surf) break; }
  if(!surf){ SDL_Log("Failed to load player spritesheet"); return 0; }
  SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(surf);
  if(!conv){ SDL_Log("ConvertSurface failed: %s", SDL_GetError()); return 0; }
  int tile_w = cfg? cfg->tile_w : 16; int tile_h = cfg? cfg->tile_h : 16; int row = cfg? cfg->row : 0;
  if(tile_w<=0) tile_w=16; if(tile_h<=0) tile_h=16; if(row<0) row=0;
  int cols = conv->w / tile_w;
  int frames = cols < max_frames ? cols : max_frames;
  for(int i=0;i<frames;i++){
    unsigned char* base = (unsigned char*)conv->pixels;
    int sx = i * tile_w; int sy = row * tile_h;
    if(sy + tile_h > conv->h) break;
    unsigned char* start = base + sy * conv->pitch + sx * 4;
    unsigned char* tmp = (unsigned char*)SDL_malloc((size_t)tile_h * (size_t)tile_w * 4);
    for(int y=0;y<tile_h;y++){
      memcpy(tmp + (size_t)y*tile_w*4, start + (size_t)y*conv->pitch, (size_t)tile_w*4);
    }
    out_frames[i] = upload_subtexture_rgba8(tmp, tile_w, tile_h, tile_w*4);
    SDL_free(tmp);
  }
  if(out_w) *out_w = tile_w;
  if(out_h) *out_h = tile_h;
  SDL_DestroySurface(conv);
  return frames;
}

void human_init(Human* h){
  if(!h) return;
  // Default animation controls centralized here
  h->cfg.tile_w = 16; h->cfg.tile_h = 16; h->cfg.row = 0;
  h->cfg.idle = 0; h->cfg.walk[0]=1; h->cfg.walk[1]=2; h->cfg.walk_count=2; h->cfg.jump=3; h->cfg.walk_fps=10.0f;

  h->hidden=0; h->frame_count=0; h->current_frame=0; h->anim_time=0.0f;
  int fw=16, fh=16;
  h->frame_count = load_player_frames(h->frames, 32, &fw, &fh, &h->cfg);
  if(h->frame_count <= 0){
    unsigned char px[16*16*4]; memset(px, 255, sizeof(px));
    h->frames[0] = upload_subtexture_rgba8(px, 16, 16, 16*4); h->frame_count = 1; fw=16; fh=16;
  }
  h->w = (float)fw; h->h = (float)fh;
  h->body = physics_create_dynamic_box(120,120,h->w,h->h,1.0f,0.4f);
}

void human_shutdown(Human* h){ (void)h; }

void human_fixed(Human* h, float dt){
  if(!h||!h->body) return;
  int dir = input_move_dir();
  float target_vx = 50.0f * (float)dir;
  bool grounded = physics_is_grounded(h->body);
  if(grounded && input_jump_edge()){
    physics_apply_impulse(h->body, 0.0f, 100.0f);
  }
  physics_set_velocity_x(h->body, target_vx);

  // Animation selection using centralized config
  if(!grounded){ h->current_frame = (h->cfg.jump < h->frame_count ? h->cfg.jump : 0); }
  else if(dir!=0 && h->cfg.walk_count>0){
    h->anim_time += dt;
    int cycle = (int)(h->anim_time * h->cfg.walk_fps) % h->cfg.walk_count;
    int idx = h->cfg.walk[cycle];
    if(idx >= 0 && idx < h->frame_count) h->current_frame = idx; else h->current_frame = h->cfg.idle;
  } else {
    h->current_frame = (h->cfg.idle < h->frame_count ? h->cfg.idle : 0);
  }
}

void human_update(Human* h, float dt){ (void)dt; (void)h; }

void human_render(const Human* h){
  if(!h||h->hidden) return;
  float x=0,y=0; physics_get_position(h->body,&x,&y);
  GLuint tex = h->frames[h->current_frame % (h->frame_count>0?h->frame_count:1)];
  pipeline_sprite_quad(x,y,h->w,h->h,tex,1,1,1,1);
}

void human_set_position(Human* h, float x, float y){ if(!h||!h->body) return; // queue teleport by setting velocity to zero and moving next fixed step
  physics_set_velocity(h->body, 0.0f, 0.0f);
  physics_teleport_body(h->body, x, y);
}
void human_get_position(const Human* h, float* x, float* y){ if(!h) return; physics_get_position(h->body,x,y); }
void human_hide(Human* h, bool hide){ if(h) h->hidden = hide?1:0; }

