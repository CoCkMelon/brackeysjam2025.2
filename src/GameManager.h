#pragma once
#include "unitylike/Scene.h"
#include "CarController.h"
#include "CarCameraController.h"
#include <vector>
#include <algorithm>
#include <glad/gl.h>
#include <box2d/box2d.h>
extern "C" {
#include "ame/physics.h"
}

using namespace unitylike;

// GameManager builds the car scene: physics, ground, car, and camera
class CarGameManager : public MongooseBehaviour {
public:
    int screenWidth = 1280;
    int screenHeight = 720;
    float gravityY = -30.0f; // tuned for car scale

private:
    GameObject car;
    GameObject cameraObj;
    GameObject groundObj;
    // We avoid storing created GameObjects to minimize ECS copying during creation
    AmePhysicsWorld* physics = nullptr;
    CarCameraController* cameraCtl = nullptr;

    // Atlas data
    GLuint atlasTex = 0;
    int atlasW = 0, atlasH = 0;
    glm::vec4 uvWheel{0,0,1,1};
    glm::vec4 uvNoise{0,0,1,1};
    glm::vec4 uvSolid{0,0,1,1};

    int obstaclesTotal = 6;
    int obstaclesSpawned = 0;

    // Build a combined atlas: circle (wheel), noise (obstacles), solid (1x1 white)
    void build_atlas() {
        // Layout: 64x32
        atlasW = 64; atlasH = 32;
        const int circleX=0, circleY=0, circleW=32, circleH=32;
        const int noiseX=32, noiseY=0, noiseW=16, noiseH=16;
        const int solidX=48, solidY=0, solidW=1, solidH=1;
        std::vector<unsigned> pix(atlasW * atlasH, 0x00000000u);

        // Circle: white opaque circle on transparent background
        for (int y=0; y<circleH; ++y) {
            for (int x=0; x<circleW; ++x) {
                float cx = (x + 0.5f) - circleW * 0.5f;
                float cy = (y + 0.5f) - circleH * 0.5f;
                float r = circleW * 0.5f - 1.0f;
                float d = sqrtf(cx*cx + cy*cy);
                unsigned a = (d <= r) ? 0xFFu : 0x00u;
                unsigned color = (a << 24) | 0x00FFFFFFu; // white with alpha
                pix[(circleY + y) * atlasW + (circleX + x)] = color;
            }
        }
        // Noise: grayscale with slight blue tint
        uint32_t seed = 1337u;
        auto rnd = [&](){ seed = seed*1664525u + 1013904223u; return seed; };
        for (int y=0; y<noiseH; ++y) {
            for (int x=0; x<noiseW; ++x) {
                uint8_t v = (uint8_t)(rnd() >> 24);
                unsigned color = (0xFFu<<24) | (v<<16) | (v<<8) | (unsigned)std::min<int>(255, v+20);
                pix[(noiseY + y) * atlasW + (noiseX + x)] = color;
            }
        }
        // Solid: 1x1 white
        pix[(solidY + 0) * atlasW + (solidX + 0)] = 0xFFFFFFFFu;

        // Upload to GL
        if (atlasTex == 0) glGenTextures(1, &atlasTex);
        glBindTexture(GL_TEXTURE_2D, atlasTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, atlasW, atlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());

        // Compute UVs for renderer, which expects v0 as top and v1 as bottom
        auto uv = [&](int x,int y,int w,int h){
            float u0 = (float)x / (float)atlasW;
            float u1 = (float)(x + w) / (float)atlasW;
            float vb = (float)y / (float)atlasH;           // bottom
            float vt = (float)(y + h) / (float)atlasH;     // top
            return glm::vec4(u0, vt, u1, vb); // v0=top, v1=bottom
        };
        uvWheel = uv(circleX, circleY, circleW, circleH);
        uvNoise = uv(noiseX, noiseY, noiseW, noiseH);
        uvSolid = uv(solidX, solidY, solidW, solidH);
    }

public:
    void Awake() {
        physics = ame_physics_world_create(0.0f, gravityY, 1.0f/60.0f);
    }

    void Start() {
        // Build atlas first so visuals can reference it
        build_atlas();

        b2World* w = physics ? (b2World*)physics->world : nullptr;
        
        // Ground physics (static) and visual
        if (w) {
            // Physics body
            b2BodyDef gd; gd.type = b2_staticBody; gd.position.Set(0.0f, 0.0f);
            b2Body* ground = w->CreateBody(&gd);
            b2PolygonShape gshape;
            float groundY = 0.0f;
            gshape.SetAsBox(100.0f, 0.5f, b2Vec2(0.0f, groundY - 0.5f), 0.0f);
            b2FixtureDef gfd; gfd.shape = &gshape; gfd.friction = 0.9f;
            ground->CreateFixture(&gfd);

            // Visual GameObject
            groundObj = gameObject().scene()->Create("Ground");
            auto& gr = groundObj.AddComponent<SpriteRenderer>();
            gr.texture(atlasTex);
            // Use 1x1 solid texel and tint to desired ground color
            gr.uv(uvSolid.x, uvSolid.y, uvSolid.z, uvSolid.w);
            gr.color({0.15f, 0.18f, 0.20f, 1.0f});
            gr.size({200.0f, 1.0f});
            gr.sortingLayer(0); gr.orderInLayer(0); gr.z(0.0f);
            // Centered at (0, groundY-0.5)
            groundObj.transform().position({0.0f, groundY - 0.5f, 0.0f});
        }

        obstaclesTotal = 6;
        obstaclesSpawned = 0;

        // Car root
        car = gameObject().scene()->Create("Car");
        auto* carCtl = &car.AddScript<CarController>();
        carCtl->SetPhysics(physics);
        carCtl->groundY = 0.0f;
        // Ensure car visuals use atlas and proper UVs (wheel: circle, body: solid)
        carCtl->ApplyAtlas(atlasTex, uvWheel, uvSolid);

        // Camera
        cameraObj = gameObject().scene()->Create("MainCamera");
        cameraCtl = &cameraObj.AddScript<CarCameraController>();
        cameraCtl->target = &car;
        cameraCtl->zoom = 8.0f; // wider view for car
        cameraCtl->SetViewport(screenWidth, screenHeight);
    }

    void FixedUpdate(float dt) {
        // Step the physics world
        if (physics) {
            ame_physics_world_step(physics);
            // Spawn one obstacle per frame until done
            b2World* w = (b2World*)physics->world;
            if (!w) return;
            if (obstaclesSpawned >= obstaclesTotal) return;

            int i = obstaclesSpawned;
            float x = -10.0f + i * 7.5f;
            float h = 1.0f + (i % 3) * 0.8f;
            float wHalf = 0.75f;
            float y = 0.5f + h * 0.5f;

            // physics body
            b2BodyDef bd; bd.type = b2_staticBody; bd.position.Set(x, y);
            b2Body* b = w->CreateBody(&bd);
            b2PolygonShape box; box.SetAsBox(wHalf, h*0.5f);
            b2FixtureDef fd; fd.shape = &box; fd.friction = 0.8f; b->CreateFixture(&fd);

            // visual
            GameObject ob = gameObject().scene()->Create("Obstacle");
            auto& sr = ob.AddComponent<SpriteRenderer>();
            sr.texture(atlasTex);
            sr.uv(uvNoise.x, uvNoise.y, uvNoise.z, uvNoise.w);
            sr.size({wHalf*2.0f, h});
            sr.sortingLayer(0); sr.orderInLayer(0); sr.z(0.0f);
            ob.transform().position({x, y, 0.0f});

            obstaclesSpawned++;
        }
    }

    void LateUpdate() {

    }

    void SetViewport(int w, int h) {
        screenWidth = w; screenHeight = h;
        if (cameraCtl) cameraCtl->SetViewport(w,h);
    }

    void OnDestroy() {
        if (physics) { ame_physics_world_destroy(physics); physics = nullptr; }
    }
};
