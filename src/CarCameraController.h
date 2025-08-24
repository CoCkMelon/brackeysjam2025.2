#pragma once
#include "unitylike/Scene.h"
#include <glm/glm.hpp>
#include <cmath>
extern "C" {
#include "ame/physics.h"
#include <SDL3/SDL.h>
}

using namespace unitylike;

// Minimal camera follow for car scene
class CarCameraController : public MongooseBehaviour {
public:
    GameObject* target = nullptr;
    float smooth = 5.0f;
    float zoom = 3.0f;

    Camera* camera = nullptr;
    float currentX = 0.0f;
    float currentY = 0.0f;

    void Awake() {
        camera = gameObject().TryGetComponent<Camera>();
        if (!camera) camera = &gameObject().AddComponent<Camera>();
        camera->zoom(zoom);
        // Set default viewport - will be updated by SetViewport
        camera->viewport(1280, 720);
    }

    void LateUpdate() {
        if (!target || !camera) return;
        auto tpos = target->transform().position();
        auto cam = camera->get();
        
        // Target position with look-ahead
        float targetX = tpos.x + 0.0f;
        float targetY = tpos.y + 2.0f; // slight look-ahead up
        
        // Initialize current position on first frame
        static bool initialized = false;
        if (!initialized) {
            currentX = targetX;
            currentY = targetY;
            initialized = true;
        }
        
        // Smooth interpolation using our smooth parameter
        float dt = Time::deltaTime();
        float smoothFactor = 1.0f - expf(-smooth * dt);
        currentX += (targetX - currentX) * smoothFactor;
        currentY += (targetY - currentY) * smoothFactor;
        
        // Set the camera target to our smoothed position
        // We'll use ame_camera_set_target but disable the engine's smoothing
        // by setting the camera position directly
        ame_camera_set_target(&cam, currentX, currentY);
        cam.x = currentX - (cam.viewport_w > 0 ? (float)cam.viewport_w : 0.0f) / (cam.zoom > 0 ? cam.zoom : 1.0f) * 0.5f;
        cam.y = currentY - (cam.viewport_h > 0 ? (float)cam.viewport_h : 0.0f) / (cam.zoom > 0 ? cam.zoom : 1.0f) * 0.5f;
        camera->set(cam);
    }

    void SetViewport(int w, int h) {
        if (!camera) return;
        camera->viewport(w, h);
    }
};
