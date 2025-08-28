#include "car.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <box2d/box2d.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "../abilities.h"
#include "../input.h"
#include "../path_util.h"
#include "../physics.h"
#include "../render/pipeline.h"

static GLuint make_color_tex(unsigned char r, unsigned char g, unsigned char b) {
    unsigned char px[4] = {r, g, b, 255};
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static GLuint load_texture_once(const char* filename) {
    SDL_Surface* surf = IMG_Load(filename);
    if (!surf) {
        return 0;
    }
    SDL_Surface* conv = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!conv) {
        SDL_Log("Failed to convert surface for %s: %s", filename, SDL_GetError());
        return 0;
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 conv->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    SDL_DestroySurface(conv);
    return tex;
}

static GLuint load_texture_from_file(const char* relpath) {
    // Try cached executable base + relative path, then parent, then CWD
    const char* base = pathutil_base();
    GLuint t = 0;
    char p0[1024];
    char p1[1024];
    if (base && base[0]) {
        SDL_snprintf(p0, sizeof(p0), "%s%s", base, relpath);
        t = load_texture_once(p0);
    }
    if (t == 0 && base && base[0]) {
        SDL_snprintf(p1, sizeof(p1), "%s../%s", base, relpath);
        t = load_texture_once(p1);
    }
    if (t == 0)
        t = load_texture_once(relpath);
    if (t == 0) {
        SDL_Log("Failed to load texture %s (tried executable-relative and CWD)", relpath);
    }
    return t;
}

void car_init(Car* c) {
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    // Defaults similar to reference
    c->cfg.body_w = 40.0f;
    c->cfg.body_h = 16.0f;
    c->cfg.wheel_radius = 6.0f;
    c->cfg.axle_offset_x_b = 10.0f;
    c->cfg.axle_offset_x_f = 12.0f;
    c->cfg.suspension_hz = 4.0f;
    c->cfg.suspension_damping = 0.7f;
    // Increase motor parameters to ensure visible wheel spin and movement
    c->cfg.motor_speed = 500.0f;      // rad/s
    c->cfg.motor_torque = 200000.0f;  // stronger torque
    c->cfg.jump_impulse = 120000.0f;
    c->cfg.boost_mul = 5.0f;
    c->cfg.fly_impulse = 12000.0f;
    c->cfg.gyro_torque = 4000000.0f;

    // Temporarly enable abilities here
    g_abilities.car_boost = true;
    // g_abilities.car_fly = true;
    g_abilities.car_jump = true;

    // Try to load texture files via executable-relative and CWD; log only once on failure
    c->tex_body = load_texture_from_file("assets/CarForBrackeyJam.png");
    if (c->tex_body == 0) {
        SDL_Log("Using fallback color texture for car body");
        c->tex_body = make_color_tex(60, 160, 255);
    }
    c->tex_wheel = load_texture_from_file("assets/CarWheelForBrackeysJam.png");
    if (c->tex_wheel == 0) {
        SDL_Log("Using fallback color texture for car wheel");
        c->tex_wheel = make_color_tex(30, 30, 30);
    }

    b2World* w = physics_get_world();
    if (!w)
        return;
    // Place car such that wheels rest on ground at y
    float base_x = 120.0f, base_y = 120.0f;
    physics_lock();
    // body
    {
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position.Set(base_x, base_y + c->cfg.wheel_radius + c->cfg.body_h * 0.5f);
        c->body = w->CreateBody(&bd);
        b2PolygonShape chassis;
        chassis.SetAsBox(c->cfg.body_w * 0.5f, c->cfg.body_h * 0.5f);
        b2FixtureDef fd;
        fd.shape = &chassis;
        fd.density = 1.0f;
        fd.friction = 0.4f;
        c->body->CreateFixture(&fd);
    }
    // wheels
    {
        float wheelY = base_y + c->cfg.wheel_radius;
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position.Set(base_x - c->cfg.axle_offset_x_b, wheelY);
        c->wheel_b = w->CreateBody(&bd);
        bd.position.Set(base_x + c->cfg.axle_offset_x_f, wheelY);
        c->wheel_f = w->CreateBody(&bd);
        b2CircleShape wheel;
        wheel.m_radius = c->cfg.wheel_radius;
        b2FixtureDef wf;
        wf.shape = &wheel;
        wf.density = 0.7f;
        wf.friction = 4.0f;
        wf.restitution = 0.0f;
        c->wheel_b->CreateFixture(&wf);
        c->wheel_f->CreateFixture(&wf);
    }
    // wheel joints
    {
        b2WheelJointDef jd;
        b2Vec2 axis(0.0f, 1.0f);
        jd.collideConnected = false;
        jd.enableLimit = true;
        jd.lowerTranslation = -50.0f;
        jd.upperTranslation = 20.0f;

        // Rear wheel suspension + motor
        jd.Initialize(c->body, c->wheel_b, c->wheel_b->GetPosition(), axis);
        jd.enableMotor = true;
        jd.motorSpeed = -c->cfg.motor_speed;
        jd.maxMotorTorque = c->cfg.motor_torque;
        // Use Box2D helper to compute stiffness/damping from freq/damping ratio
        b2LinearStiffness(jd.stiffness, jd.damping, c->cfg.suspension_hz, c->cfg.suspension_damping,
                          c->body, c->wheel_b);
        c->joint_b = (b2WheelJoint*)physics_get_world()->CreateJoint(&jd);

        // Front wheel suspension (free rolling)
        jd.Initialize(c->body, c->wheel_f, c->wheel_f->GetPosition(), axis);
        jd.enableMotor = false;
        b2LinearStiffness(jd.stiffness, jd.damping, c->cfg.suspension_hz, c->cfg.suspension_damping,
                          c->body, c->wheel_f);
        c->joint_f = (b2WheelJoint*)physics_get_world()->CreateJoint(&jd);
    }
    physics_unlock();
}

void car_shutdown(Car* c) {
    if (!c)
        return;
    // Clean up textures
    if (c->tex_body != 0) {
        glDeleteTextures(1, &c->tex_body);
        c->tex_body = 0;
    }
    if (c->tex_wheel != 0) {
        glDeleteTextures(1, &c->tex_wheel);
        c->tex_wheel = 0;
    }
}

void car_fixed(Car* c, float dt) {
    (void)dt;
    if (!c || !c->body)
        return;
    if (c->pending_teleport) {
        physics_teleport_body(c->body, c->pending_tx,
                              c->pending_ty + c->cfg.wheel_radius + c->cfg.body_h * 0.5f);
        c->pending_teleport = 0;
    }
    // Use W/S for acceleration, A/D for yaw/spin
    int accel = input_accel_dir();
    int yaw = input_yaw_dir();
    float boost = (g_abilities.car_boost && input_boost_down()) ? c->cfg.boost_mul : 1.0f;
    float speed = -c->cfg.motor_speed * boost * (float)accel;
    physics_lock();
    if (c->joint_b) {
        c->joint_b->EnableMotor(true);
        c->joint_b->SetMaxMotorTorque(c->cfg.motor_torque);
        c->joint_b->SetMotorSpeed(speed);
    }
    if (c->joint_f) {
        c->joint_f->EnableMotor(true);
        c->joint_f->SetMaxMotorTorque(c->cfg.motor_torque);
        c->joint_f->SetMotorSpeed(speed);
    }
    // Apply yaw torque regardless of grounded state
    float torque = -c->cfg.gyro_torque * (float)yaw;  // stronger yaw torque for noticeable rotation
    c->body->ApplyTorque(torque, true);
    // Jump/hop (unlockable) - grounded condition only needed for jumping
    if (g_abilities.car_jump && input_jump_edge() && physics_is_grounded(c->body)) {
        c->body->ApplyLinearImpulseToCenter(b2Vec2(0.0f, c->cfg.jump_impulse), true);
    }
    // Helicopter-like fly (unlockable): hold Space to get gentle upward force
    if (g_abilities.car_fly && input_jump_down()) {
        c->body->ApplyForceToCenter(b2Vec2(0.0f, c->cfg.fly_impulse), true);
    }
    physics_unlock();
}

void car_update(Car* c, float dt) {
    (void)c;
    (void)dt;
}

void car_render(const Car* c) {
    if (!c || !c->body)
        return;
    physics_lock();
    b2Vec2 p = c->body->GetPosition();
    float ang = c->body->GetAngle();
    pipeline_sprite_quad_rot(p.x, p.y, c->cfg.body_w, c->cfg.body_h, ang, c->tex_body, 1, 1, 1, 1);
    if (c->wheel_b) {
        b2Vec2 wb = c->wheel_b->GetPosition();
        float d = c->cfg.wheel_radius * 2.0f;
        float wang = c->wheel_b->GetAngle();
        pipeline_sprite_quad_rot(wb.x, wb.y, d, d, wang, c->tex_wheel, 1, 1, 1, 1);
    }
    if (c->wheel_f) {
        b2Vec2 wf = c->wheel_f->GetPosition();
        float d = c->cfg.wheel_radius * 2.0f;
        float wang = c->wheel_f->GetAngle();
        pipeline_sprite_quad_rot(wf.x, wf.y, d, d, wang, c->tex_wheel, 1, 1, 1, 1);
    }
    physics_unlock();
}

void car_set_position(Car* c, float x, float y) {
    if (!c)
        return;
    // If the Box2D body is already created, teleport immediately so gameplay logic
    // (like switching distance checks) sees the correct position right away.
    if (c->body) {
        physics_lock();
        physics_teleport_body(c->body, x, y + c->cfg.wheel_radius + c->cfg.body_h * 0.5f);
        // Keep wheels roughly aligned with body height so suspension settles quickly
        if (c->wheel_b) {
            physics_teleport_body(c->wheel_b, x - c->cfg.axle_offset_x_b, y + c->cfg.wheel_radius);
        }
        if (c->wheel_f) {
            physics_teleport_body(c->wheel_f, x + c->cfg.axle_offset_x_f, y + c->cfg.wheel_radius);
        }
        physics_unlock();
        c->pending_teleport = 0;
    } else {
        c->pending_teleport = 1;
        c->pending_tx = x;
        c->pending_ty = y;
    }
}
void car_get_position(const Car* c, float* x, float* y) {
    if (!c || !c->body) {
        if (x)
            *x = 0;
        if (y)
            *y = 0;
        return;
    }
    physics_lock();
    b2Vec2 p = c->body->GetPosition();
    if (x)
        *x = p.x;
    if (y)
        *y = p.y;
    physics_unlock();
}

float car_get_rear_wheel_angular_speed(const Car* c) {
    if (!c || !c->wheel_b)
        return 0.0f;
    physics_lock();
    // Get the angular velocity of the rear wheel in rad/s
    float angular_speed = fabsf(c->wheel_b->GetAngularVelocity());  // absolute value for audio
    physics_unlock();
    return angular_speed;
}

float car_get_front_wheel_angular_speed(const Car* c) {
    if (!c || !c->wheel_f)
        return 0.0f;
    physics_lock();
    // Get the angular velocity of the front wheel in rad/s
    float angular_speed = fabsf(c->wheel_f->GetAngularVelocity());  // absolute value for audio
    physics_unlock();
    return angular_speed;
}
