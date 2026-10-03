#pragma once

#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdio>

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <cstdint>
#include <exception>

#include "d3dx12.h"
#include "Shapes.h"

#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../math/Mat4.h"

#include <cmath>
#include <numbers>

inline float g_CubeAngle = 0.5f;

inline vec3 g_CameraPosition = { 0.0f, 0.0f, -3.0f };

inline float g_CameraYaw = 0.0f;
inline float g_CameraPitch = 0.0f;

// This might work better if it's a macro or something else than a function
inline float radians(float degrees) {
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}

inline vec3 GetCameraForward() {
    return vec3(
        std::sin(g_CameraYaw) * std::cos(g_CameraPitch),
        std::sin(g_CameraPitch),
        std::cos(g_CameraYaw) * std::cos(g_CameraPitch)
    );
}

inline mat4 GetCameraView() {
    vec3 forward = GetCameraForward();
    vec3 right = normalize(vec3(0.0f, 1.0f, 0.0f).cross(forward));
    vec3 up = forward.cross(right);

    return mat4(
        vec4(right.x, up.x, forward.x, 0.0f),
        vec4(right.y, up.y, forward.y, 0.0f),
        vec4(right.z, up.z, forward.z, 0.0f),
        vec4(-right.dot(g_CameraPosition),
            -up.dot(g_CameraPosition),
            -forward.dot(g_CameraPosition), 1.0f));
}

inline mat4 GetCameraPosition(uint32_t& g_ClientWidth, uint32_t& g_ClientHeight) {
    float fov = radians(60.0f);

    float aspect = float(g_ClientWidth) / float(g_ClientHeight);
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float scale = 1.0f / std::tan(fov * 0.5f);

    float depth = farPlane / (farPlane - nearPlane);

    return mat4(
        vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
        vec4(0.0f, scale, 0.0f, 0.0f),
        vec4(0.0f, 0.0f, depth, 1.0f),
        vec4(0.0f, 0.0f, -nearPlane * depth, 0.0f));
}