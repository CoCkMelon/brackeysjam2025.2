#pragma once
#include "input_local.h"
#include "unitylike/Scene.h"
#include <glad/gl.h>
#include <box2d/box2d.h>
#include <cmath>
extern "C" {
#include "ame/physics.h"
}
#include <vector>

using namespace unitylike;

class CarController : public MongooseBehaviour {
public:
    // Config
    float bodyWidth = 2.5f;   // world units
    float bodyHeight = 1.0f;
    float wheelRadius = 0.5f;
    float motorSpeed = 30.0f;   // rad/s
    float motorTorque = 50.0f;
    float drive = 1.0f;         // -1..1 from input
    float spinning = 1.0f;         // -1..1 from input
    float suspensionHz = 4.0f;
    float suspensionDamping = 0.7f;

    // Ground config
    float groundY = 0.0f;

private:
    AmePhysicsWorld* physics = nullptr;
    b2Body* body = nullptr;
    b2Body* wheelB = nullptr; // front-left (left in 2D)
    b2Body* wheelF = nullptr;
    b2WheelJoint* jointFL = nullptr;
    b2WheelJoint* jointFR = nullptr;

    // GameObjects for visuals
    GameObject bodyObj;
    GameObject wheelBO;
    GameObject wheelFO;

    // Renderers
    SpriteRenderer* wheelBRenderer = nullptr;
    SpriteRenderer* wheelFRenderer = nullptr;
    SpriteRenderer* bodyRenderer = nullptr;

    // Shared atlas state (assigned by GameManager)
    GLuint atlasTex = 0;
    glm::vec4 wheelUV{0,0,1,1};
    glm::vec4 bodyUV{0,0,1,1};

public:
    void SetPhysics(AmePhysicsWorld* w) { physics = w; }

    // Apply a shared atlas and UVs for wheels/body. Call after sprites are created.
    void ApplyAtlas(GLuint tex, const glm::vec4& wheel_uv, const glm::vec4& body_uv) {
        atlasTex = tex; wheelUV = wheel_uv; bodyUV = body_uv;
        if (wheelBRenderer) { wheelBRenderer->texture(atlasTex); wheelBRenderer->uv(wheelUV.x, wheelUV.y, wheelUV.z, wheelUV.w); }
        if (wheelFRenderer) { wheelFRenderer->texture(atlasTex); wheelFRenderer->uv(wheelUV.x, wheelUV.y, wheelUV.z, wheelUV.w); }
        if (bodyRenderer)   { bodyRenderer->texture(atlasTex);   bodyRenderer->uv(bodyUV.x, bodyUV.y, bodyUV.z, bodyUV.w); }
    }

    void Awake() override {
        // Visual GameObjects
        wheelBO = gameObject().scene()->Create("WheelFL");
        wheelFO = gameObject().scene()->Create("WheelFR");
        bodyObj = gameObject().scene()->Create("CarBodyVisual");

        // Wheel sprites (start with texture 0; GameManager will assign atlas+UVs)
        auto& sr1 = wheelBO.AddComponent<SpriteRenderer>();
        sr1.texture(0); sr1.size({wheelRadius*2, wheelRadius*2}); sr1.sortingLayer(0); sr1.orderInLayer(0); sr1.z(0.0f);
        wheelBRenderer = &sr1;
        auto& sr2 = wheelFO.AddComponent<SpriteRenderer>();
        sr2.texture(0); sr2.size({wheelRadius*2, wheelRadius*2}); sr2.sortingLayer(0); sr2.orderInLayer(0); sr2.z(0.0f);
        wheelFRenderer = &sr2;

        // Car body as colored rectangle using SpriteRenderer
        auto& bs = bodyObj.AddComponent<SpriteRenderer>();
        bs.texture(0);
        bs.size({bodyWidth, bodyHeight});
        bs.color({0.2f, 0.6f, 1.0f, 1.0f});
        bs.sortingLayer(0); bs.orderInLayer(0); bs.z(0.0f);
        bodyRenderer = &bs;

        // If atlas was already provided before Awake, apply it now
        if (atlasTex != 0) {
            ApplyAtlas(atlasTex, wheelUV, bodyUV);
        }
    }

    void Start() override {
        if (!physics) return;
        b2World* w = (b2World*)physics->world;
        if (!w) return;
        
        // Car body: place so that bottom of body aligns with wheel center at start
        float bodyY = groundY + wheelRadius + bodyHeight * 0.5f; // ensures bottom = wheelY
        {
            b2BodyDef bd;
            bd.type = b2_dynamicBody;
            bd.position.Set(0.0f, bodyY);
            body = w->CreateBody(&bd);
            if (!body) return;

            b2PolygonShape chassis;
            chassis.SetAsBox(bodyWidth * 0.5f, bodyHeight * 0.5f);
            b2FixtureDef bodyFd;
            bodyFd.shape = &chassis;
            bodyFd.density = 1.0f;
            bodyFd.friction = 0.4f;
            body->CreateFixture(&bodyFd);
        }

        // Wheels positioning
        float axleOffsetX = bodyWidth*0.35f;
        b2Vec2 bodyCenter = body->GetPosition();
        
        // Position wheels at ground level where they should naturally rest
        float wheelX_B = bodyCenter.x - axleOffsetX;
        float wheelX_F = bodyCenter.x + axleOffsetX;
        float wheelY = groundY + wheelRadius; // Place wheels on the ground
        
        // Create wheel bodies + circle fixtures
        {
            b2BodyDef wbd;
            wbd.type = b2_dynamicBody;
            
            wbd.position.Set(wheelX_B, wheelY);
            wheelB = w->CreateBody(&wbd);
            
            wbd.position.Set(wheelX_F, wheelY);
            wheelF = w->CreateBody(&wbd);

            if (!wheelB || !wheelF) return;

            b2CircleShape wheelShape;
            wheelShape.m_radius = wheelRadius;
            b2FixtureDef wf;
            wf.shape = &wheelShape;
            wf.density = 0.7f;
            wf.friction = 1.2f; // traction
            wheelB->CreateFixture(&wf);
            wheelF->CreateFixture(&wf);
        }

        // Create wheel joints via Box2D C++ API
        b2WheelJointDef jd;
        b2Vec2 axis(0.0f, 1.0f); // suspension axis vertical
        
        // Joint anchor points should align with wheel centers for proper rotation
        b2Vec2 anchorB = b2Vec2(wheelX_B, wheelY); // Same position as wheel center
        b2Vec2 anchorF = b2Vec2(wheelX_F, wheelY); // Same position as wheel center

        // Back wheel joint
        jd.Initialize(body, wheelB, anchorB, axis);
        jd.enableMotor = true;
        jd.motorSpeed = -motorSpeed; // will scale by input
        jd.maxMotorTorque = motorTorque;
        jd.stiffness = suspensionHz * suspensionHz * body->GetMass() * 0.5f; // Spring stiffness
        jd.damping = 2.0f * suspensionDamping * sqrtf(jd.stiffness * body->GetMass());
        jd.lowerTranslation = -0.5f; // Max compression
        jd.upperTranslation = 0.2f;  // Max extension
        jd.enableLimit = true;
        jointFL = (b2WheelJoint*)w->CreateJoint(&jd);

        // Front wheel joint
        jd.Initialize(body, wheelF, anchorF, axis);
        jointFR = (b2WheelJoint*)w->CreateJoint(&jd);
    }

    void FixedUpdate(float dt) {
        // Check if physics body is properly initialized
        if (!body || !physics) return;
        
        // Input: simple up/down arrows via asyncinput wrapper
        int dir = input_move_dir();
        spinning = input_yaw_dir();
        drive = (float)dir;
        float speed = -motorSpeed * drive; // sign for right-hand coord
        if (jointFL) jointFL->SetMotorSpeed(speed);
        if (jointFR) jointFR->SetMotorSpeed(speed);
        
        // Apply torque for body rotation (much gentler)
        body->ApplyTorque(spinning * 500.0f, true);
        
        // Add some angular damping to prevent excessive spinning
        float angularVel = body->GetAngularVelocity();
        if (abs(spinning) < 0.1f) { // Only damp when not actively steering
            body->SetAngularDamping(0.8f);
        } else {
            body->SetAngularDamping(0.1f);
        }

        // Sync visuals
        sync_visuals();
    }

private:
    void sync_visuals() {
        if (!physics) return;
        if (bodyRenderer && body) {
            b2Vec2 bpos = body->GetPosition();
            float angle = body->GetAngle(); // Use Box2D C++ API
            bodyObj.transform().position({bpos.x, bpos.y, 0.0f});
            bodyObj.transform().rotation(glm::angleAxis(angle, glm::vec3(0, 0, 1)));
            
            // Also sync the main car GameObject transform so camera can follow it
            gameObject().transform().position({bpos.x, bpos.y, 0.0f});
            gameObject().transform().rotation(glm::angleAxis(angle, glm::vec3(0, 0, 1)));
        }
        if (wheelBRenderer && wheelB) {
            b2Vec2 wpos = wheelB->GetPosition();
            float angle = wheelB->GetAngle(); // Use Box2D C++ API
            wheelBO.transform().position({wpos.x, wpos.y, 0.0f});
            wheelBO.transform().rotation(glm::angleAxis(angle, glm::vec3(0, 0, 1)));
        }
        if (wheelFRenderer && wheelF) {
            b2Vec2 wpos = wheelF->GetPosition();
            float angle = wheelF->GetAngle(); // Use Box2D C++ API
            wheelFO.transform().position({wpos.x, wpos.y, 0.0f});
            wheelFO.transform().rotation(glm::angleAxis(angle, glm::vec3(0, 0, 1)));
        }
    }
};
