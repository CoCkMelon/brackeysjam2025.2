#include "car.h"
#include "../physics.h"
#include "../input.h"
#include "../render/pipeline.h"
#include "../abilities.h"
#include <SDL3/SDL.h>
#include <string.h>
#include <box2d/box2d.h>

static GLuint make_color_tex(unsigned char r,unsigned char g,unsigned char b){
  unsigned char px[4]={r,g,b,255}; GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  return t;
}

void car_init(Car* c){
  if(!c) return; memset(c,0,sizeof(*c));
  // Defaults similar to reference
  c->cfg.body_w=40.0f; c->cfg.body_h=16.0f;
  c->cfg.wheel_radius=6.0f; c->cfg.axle_offset_x=12.0f;
  c->cfg.suspension_hz=4.0f; c->cfg.suspension_damping=0.7f;
  c->cfg.motor_speed=30.0f; c->cfg.motor_torque=50.0f;
  c->cfg.jump_impulse=120.0f; c->cfg.boost_mul=1.8f;
  c->tex_body = make_color_tex(60,160,255);
  c->tex_wheel = make_color_tex(30,30,30);

  b2World* w = physics_get_world(); if(!w) return;
  // Place car such that wheels rest on ground at y
  float base_x=120.0f, base_y=120.0f;
  // body
  {
    b2BodyDef bd; bd.type=b2_dynamicBody; bd.position.Set(base_x, base_y + c->cfg.wheel_radius + c->cfg.body_h*0.5f);
    c->body = w->CreateBody(&bd);
    b2PolygonShape chassis; chassis.SetAsBox(c->cfg.body_w*0.5f, c->cfg.body_h*0.5f);
    b2FixtureDef fd; fd.shape=&chassis; fd.density=1.0f; fd.friction=0.4f;
    c->body->CreateFixture(&fd);
  }
  // wheels
  {
    float wheelY = base_y + c->cfg.wheel_radius;
    b2BodyDef bd; bd.type=b2_dynamicBody;
    bd.position.Set(base_x - c->cfg.axle_offset_x, wheelY); c->wheel_b = w->CreateBody(&bd);
    bd.position.Set(base_x + c->cfg.axle_offset_x, wheelY); c->wheel_f = w->CreateBody(&bd);
    b2CircleShape wheel; wheel.m_radius = c->cfg.wheel_radius;
    b2FixtureDef wf; wf.shape=&wheel; wf.density=0.7f; wf.friction=2.0f; wf.restitution=0.0f;
    c->wheel_b->CreateFixture(&wf); c->wheel_f->CreateFixture(&wf);
  }
  // wheel joints
  {
    b2WheelJointDef jd; b2Vec2 axis(0.0f, 1.0f);
    jd.collideConnected = false;
    jd.enableLimit = true;
    jd.lowerTranslation = -0.5f;
    jd.upperTranslation =  0.2f;

    // Rear wheel suspension + motor
    jd.Initialize(c->body, c->wheel_b, c->wheel_b->GetPosition(), axis);
    jd.enableMotor = true;
    jd.motorSpeed = -c->cfg.motor_speed;
    jd.maxMotorTorque = c->cfg.motor_torque;
    // Use Box2D helper to compute stiffness/damping from freq/damping ratio
    b2LinearStiffness(jd.stiffness, jd.damping, c->cfg.suspension_hz, c->cfg.suspension_damping, c->body, c->wheel_b);
    c->joint_b = (b2WheelJoint*)physics_get_world()->CreateJoint(&jd);

    // Front wheel suspension (free rolling)
    jd.Initialize(c->body, c->wheel_f, c->wheel_f->GetPosition(), axis);
    jd.enableMotor = false;
    b2LinearStiffness(jd.stiffness, jd.damping, c->cfg.suspension_hz, c->cfg.suspension_damping, c->body, c->wheel_f);
    c->joint_f = (b2WheelJoint*)physics_get_world()->CreateJoint(&jd);
  }
}

void car_shutdown(Car* c){ (void)c; }

void car_fixed(Car* c, float dt){
  (void)dt; if(!c||!c->body) return;
  int dir = input_move_dir(); float boost = (g_abilities.car_boost && input_boost_down())? c->cfg.boost_mul : 1.0f;
  float speed = -c->cfg.motor_speed * boost * (float)dir;
  if(c->joint_b) c->joint_b->SetMotorSpeed(speed);
  if(c->joint_f) c->joint_f->SetMotorSpeed(speed);
  // Jump/hop (unlockable)
  if(g_abilities.car_jump && input_jump_edge() && physics_is_grounded(c->body)){
    c->body->ApplyLinearImpulseToCenter(b2Vec2(0.0f, c->cfg.jump_impulse), true);
  }
  // Helicopter-like fly (unlockable): hold Space to get gentle upward force
  if(g_abilities.car_fly && input_jump_down()){
    c->body->ApplyForceToCenter(b2Vec2(0.0f, c->cfg.jump_impulse * 0.6f), true);
  }
}

void car_update(Car* c, float dt){ (void)c; (void)dt; }

void car_render(const Car* c){
  if(!c||!c->body) return;
  b2Vec2 p = c->body->GetPosition();
  pipeline_sprite_quad(p.x, p.y, c->cfg.body_w, c->cfg.body_h, c->tex_body, 1,1,1,1);
  if(c->wheel_b){ b2Vec2 wb=c->wheel_b->GetPosition(); float d=c->cfg.wheel_radius*2.0f; pipeline_sprite_quad(wb.x, wb.y, d, d, c->tex_wheel,1,1,1,1);} 
  if(c->wheel_f){ b2Vec2 wf=c->wheel_f->GetPosition(); float d=c->cfg.wheel_radius*2.0f; pipeline_sprite_quad(wf.x, wf.y, d, d, c->tex_wheel,1,1,1,1);} 
}

void car_set_position(Car* c, float x, float y){ if(!c||!c->body) return; c->body->SetTransform(b2Vec2(x, y + c->cfg.wheel_radius + c->cfg.body_h*0.5f), c->body->GetAngle()); }
void car_get_position(const Car* c, float* x, float* y){ if(!c||!c->body){ if(x)*x=0; if(y)*y=0; return; } b2Vec2 p=c->body->GetPosition(); if(x)*x=p.x; if(y)*y=p.y; }

