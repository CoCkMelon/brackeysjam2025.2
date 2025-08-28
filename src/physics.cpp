#include "physics.h"
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include <stdlib.h>
#include <atomic>

struct FixtureUserData { bool is_ground; bool is_sensor; };

static b2World* g_world = nullptr;
static SDL_Thread* g_thread = NULL;
static std::atomic_bool g_running{false};
static float g_dt = 0.001f; // 1000 Hz

static int physics_thread(void* ud){ (void)ud; 
  uint64_t last = SDL_GetTicksNS(); double acc=0.0; 
  g_running.store(true);
  while(g_running.load()){
    uint64_t t = SDL_GetTicksNS(); double frame = (double)(t-last)/1e9; last=t; if(frame>0.05) frame=0.05; acc+=frame;
    int steps=0; while(acc >= g_dt && steps<8){ if(g_world) g_world->Step(g_dt, 8, 3); acc-=g_dt; steps++; }
    SDL_DelayNS(200000); // ~0.2ms
  }
  return 0; }

bool physics_init(void){
  b2Vec2 gravity(0.0f, -100.0f);
  g_world = new b2World(gravity);
  g_thread = SDL_CreateThread(physics_thread, "phys", NULL);
  return g_world && g_thread; }

void physics_shutdown(void){
  g_running.store(false);
  if(g_thread){ SDL_WaitThread(g_thread,NULL); g_thread=NULL; }
  delete g_world; g_world=nullptr; }

b2Body* physics_create_dynamic_box(float x, float y, float w, float h, float density, float friction){
  if(!g_world) return nullptr; b2BodyDef bd; bd.type=b2_dynamicBody; bd.position.Set(x,y); b2Body* b = g_world->CreateBody(&bd);
  b2PolygonShape sh; sh.SetAsBox(w*0.5f, h*0.5f); b2FixtureDef fd; fd.shape=&sh; fd.density=density; fd.friction=friction; b->CreateFixture(&fd); return b; }

void physics_create_static_box(float x, float y, float w, float h, float friction){
  if(!g_world) return; b2BodyDef bd; bd.type=b2_staticBody; bd.position.Set(x,y); b2Body* b = g_world->CreateBody(&bd);
  b2PolygonShape sh; sh.SetAsBox(w*0.5f, h*0.5f); b2FixtureDef fd; fd.shape=&sh; fd.friction=friction; b->CreateFixture(&fd); }

void physics_apply_impulse(b2Body* body, float ix, float iy){ if(!body) return; body->ApplyLinearImpulseToCenter(b2Vec2(ix,iy), true); }
void physics_set_velocity(b2Body* body, float vx, float vy){ if(!body) return; body->SetLinearVelocity(b2Vec2(vx,vy)); }
void physics_set_velocity_x(b2Body* body, float vx){ if(!body) return; b2Vec2 v = body->GetLinearVelocity(); v.x = vx; body->SetLinearVelocity(v); }
void physics_get_position(b2Body* body, float* x, float* y){ if(!body) { if(x)*x=0; if(y)*y=0; return; } b2Vec2 p=body->GetPosition(); if(x)*x=p.x; if(y)*y=p.y; }
void physics_get_velocity(b2Body* body, float* vx, float* vy){ if(!body){ if(vx)*vx=0; if(vy)*vy=0; return; } b2Vec2 v=body->GetLinearVelocity(); if(vx)*vx=v.x; if(vy)*vy=v.y; }

bool physics_is_grounded_ex(b2Body* body, float normal_threshold, float max_upward_velocity){
  if(!body) return false;
  b2Vec2 vel = body->GetLinearVelocity();
  if(vel.y > max_upward_velocity) return false;
  for(b2ContactEdge* ce = body->GetContactList(); ce; ce = ce->next){
    b2Contact* c = ce->contact; if(!c->IsTouching()) continue;
    b2Fixture* a = c->GetFixtureA(); b2Fixture* b = c->GetFixtureB();
    b2Fixture* my = (a->GetBody()==body)? a : b;
    b2Fixture* other = (a->GetBody()==body)? b : a;
#if defined(b2_api_H)
    // Box2D C++ 2.4 user data API
    FixtureUserData* my_ud = reinterpret_cast<FixtureUserData*>(my->GetUserData().pointer);
    FixtureUserData* ot_ud = reinterpret_cast<FixtureUserData*>(other->GetUserData().pointer);
#else
    FixtureUserData* my_ud = NULL; FixtureUserData* ot_ud = NULL;
#endif
    if(!my_ud || !my_ud->is_sensor) continue;
    if(!ot_ud || !ot_ud->is_ground) continue;
    b2WorldManifold wm; c->GetWorldManifold(&wm);
    b2Vec2 n = wm.normal; if(n.y > normal_threshold) return true;
  }
  return false;
}

bool physics_is_grounded(b2Body* body){ return physics_is_grounded_ex(body, 0.5f, 0.0f); }

void physics_teleport_body(b2Body* body, float x, float y){ if(!body) return; body->SetTransform(b2Vec2(x,y), body->GetAngle()); body->SetLinearVelocity(b2Vec2(0,0)); body->SetAngularVelocity(0); }

void physics_set_gravity(float gx, float gy){ if(!g_world) return; g_world->SetGravity(b2Vec2(gx,gy)); }

b2World* physics_get_world(void){ return g_world; }

