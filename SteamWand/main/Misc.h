#pragma once

#include "../rendering/App.h"

inline void Freecam() {
    if (inputHandler.isKeyDown(Button::MouseRight) && !inputHandler.isKeyPressed(Button::MouseRight)) {
        float sensitivity = 0.003f;

        g_CameraYaw += (inputHandler.mouseX - inputHandler.previousMouseX) * sensitivity;
        g_CameraPitch -= (inputHandler.mouseY - inputHandler.previousMouseY) * sensitivity;

        g_CameraPitch = std::clamp(g_CameraPitch, -1.5f, 1.5f);
    }

    if (inputHandler.isKeyDown(Button::MouseRight)) {
        vec3 forward = GetCameraForward();
        vec3 right(std::cos(g_CameraYaw), 0.0f, -std::sin(g_CameraYaw));

        vec3 movement(0.0f, 0.0f, 0.0f);

        float step = 3.0f * GetDeltaTime();

        if (inputHandler.isKeyDown(Button::W)) movement += forward;
        if (inputHandler.isKeyDown(Button::S)) movement -= forward;
        if (inputHandler.isKeyDown(Button::A)) movement -= right;
        if (inputHandler.isKeyDown(Button::D)) movement += right;

        if (movement.dot(movement) > 0.0f)
            g_CameraPosition += normalize(movement) * step;
    }
}