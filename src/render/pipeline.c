#include "pipeline.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "ame/camera.h"

// Simple shader combining sprite and textured mesh in one pass style
static const char* VS =
  "#version 450 core\n"
  "layout(location=0) in vec2 a_pos;\n"
  "layout(location=1) in vec4 a_col;\n"
  "layout(location=2) in vec2 a_uv;\n"
  "uniform vec2 u_res;\n"
  "uniform vec4 u_cam; // x,y,zoom,rot (rot unused)\n"
  "out vec4 v_col;\n"
  "out vec2 v_uv;\n"
  "void main(){\n"
  "  // Y-up bottom-left centered coordinates: camera.xy is bottom-left in world px\n"
  "  vec2 p = a_pos - u_cam.xy;\n"
  "  p *= u_cam.z;\n"
  "  vec2 ndc = vec2((p.x/u_res.x)*2.0 - 1.0, (p.y/u_res.y)*2.0 - 1.0);\n"
  "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
  "  v_col = a_col;\n"
  "  v_uv = vec2(a_uv.x, 1.0 - a_uv.y);\n"
  "}\n";

static const char* FS =
  "#version 450 core\n"
  "in vec4 v_col;\n"
  "in vec2 v_uv;\n"
  "uniform bool u_use_tex;\n"
  "uniform sampler2D u_tex;\n"
  "out vec4 frag;\n"
  "void main(){\n"
  "  vec4 c = v_col;\n"
  "  if(u_use_tex){ c *= texture(u_tex, v_uv); }\n"
  "  frag = c;\n"
  "}\n";

typedef struct { float x,y,u,v,r,g,b,a; } Vtx;

typedef struct { GLuint prog, vao, vbo; GLint u_res, u_cam, u_use_tex, u_tex; } Pipe;
static Pipe g_p;

static GLuint compile(GLenum t, const char* s){ GLuint sh=glCreateShader(t); glShaderSource(sh,1,&s,NULL); glCompileShader(sh); GLint ok=0; glGetShaderiv(sh,GL_COMPILE_STATUS,&ok); if(!ok){ char log[1024]; GLsizei n=0; glGetShaderInfoLog(sh,1024,&n,log); SDL_Log("shader: %.*s",(int)n,log);} return sh; }
static GLuint linkp(GLuint vs, GLuint fs){ GLuint p=glCreateProgram(); glAttachShader(p,vs); glAttachShader(p,fs); glLinkProgram(p); GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok); if(!ok){ char log[1024]; GLsizei n=0; glGetProgramInfoLog(p,1024,&n,log); SDL_Log("prog: %.*s",(int)n,log);} return p; }

bool pipeline_init(void){
  GLuint vs=compile(GL_VERTEX_SHADER,VS); GLuint fs=compile(GL_FRAGMENT_SHADER,FS); g_p.prog=linkp(vs,fs); glDeleteShader(vs); glDeleteShader(fs);
  glGenVertexArrays(1,&g_p.vao);
  glGenBuffers(1,&g_p.vbo);
  glBindVertexArray(g_p.vao);
  glBindBuffer(GL_ARRAY_BUFFER,g_p.vbo);
  glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0); glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)offsetof(Vtx,x));
  glEnableVertexAttribArray(1); glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)offsetof(Vtx,r));
  glEnableVertexAttribArray(2); glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vtx),(void*)offsetof(Vtx,u));
  g_p.u_res = glGetUniformLocation(g_p.prog,"u_res");
  g_p.u_cam = glGetUniformLocation(g_p.prog,"u_cam");
  g_p.u_use_tex = glGetUniformLocation(g_p.prog,"u_use_tex");
  g_p.u_tex = glGetUniformLocation(g_p.prog,"u_tex");
  return g_p.prog!=0;
}

void pipeline_shutdown(void){
  if(g_p.vbo){ GLuint b=g_p.vbo; glDeleteBuffers(1,&b); g_p.vbo=0; }
  if(g_p.vao){ GLuint a=g_p.vao; glDeleteVertexArrays(1,&a); g_p.vao=0; }
  if(g_p.prog){ GLuint p=g_p.prog; glDeleteProgram(p); g_p.prog=0; }
}

static int g_vw=0,g_vh=0; static AmeCamera g_cam;
static Vtx* g_batch=NULL; static size_t g_cap=0, g_len=0; 

static void batch_reserve(size_t add){ size_t need=g_len+add; if(need>g_cap){ size_t nc = g_cap? g_cap*2: 1024; while(nc<need) nc*=2; g_batch=(Vtx*)realloc(g_batch,nc*sizeof(Vtx)); g_cap=nc; } }

void pipeline_begin(const AmeCamera *cam, int vw, int vh){
  g_vw=vw; g_vh=vh; if(cam) g_cam=*cam; g_len=0;
  glUseProgram(g_p.prog);
  glBindVertexArray(g_p.vao);
  if(g_p.u_res>=0) glUniform2f(g_p.u_res,(float)vw,(float)vh);
  float snapped_x = floorf(g_cam.x + 0.5f); float snapped_y = floorf(g_cam.y + 0.5f);
  if(g_p.u_cam>=0) glUniform4f(g_p.u_cam, snapped_x, snapped_y, g_cam.zoom, g_cam.rotation);
}

void pipeline_sprite_quad(float cx, float cy, float w, float h, unsigned int tex, float r, float g, float b, float a){
  float x0=cx - w*0.5f; float y0=cy - h*0.5f; float x1=cx + w*0.5f; float y1=cy + h*0.5f;
  batch_reserve(6);
  Vtx q[6]={
    {x0,y0,0,0,r,g,b,a},{x1,y0,1,0,r,g,b,a},{x0,y1,0,1,r,g,b,a},
    {x1,y0,1,0,r,g,b,a},{x1,y1,1,1,r,g,b,a},{x0,y1,0,1,r,g,b,a}
  };
  memcpy(g_batch+g_len,q,sizeof(q)); g_len+=6;
  // bind and draw immediately per-texture for simplicity
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
}

void pipeline_mesh_submit(const AmeLocalMesh *mesh, float tx, float ty, float sx, float sy, float r, float g, float b, float a){
  if(!mesh || mesh->count==0 || !mesh->pos) return;
  // Expand to Vtx and append
  batch_reserve(mesh->count);
  for(unsigned int i=0;i<mesh->count;i++){
    float px = mesh->pos[i*2+0]*sx + tx;
    float py = mesh->pos[i*2+1]*sy + ty;
    float u = mesh->uv? mesh->uv[i*2+0] : 0.0f;
    float v = mesh->uv? mesh->uv[i*2+1] : 0.0f;
    g_batch[g_len++] = (Vtx){px,py,u,v,r,g,b,a};
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, mesh->texture);
}

void pipeline_end(void){
  if(g_len==0) return;
  glBindBuffer(GL_ARRAY_BUFFER, g_p.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(g_len*sizeof(Vtx)), g_batch, GL_DYNAMIC_DRAW);
  if(g_p.u_tex>=0) glUniform1i(g_p.u_tex, 0);
  if(g_p.u_use_tex>=0) glUniform1i(g_p.u_use_tex, 1);
  glDrawArrays(GL_TRIANGLES, 0, (GLsizei)g_len);
}

