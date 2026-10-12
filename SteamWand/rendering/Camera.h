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

inline vec3 GetCameraForward() {
    return vec3(
        std::sin(g_CameraYaw) * std::cos(g_CameraPitch),
        std::sin(g_CameraPitch),
        std::cos(g_CameraYaw) * std::cos(g_CameraPitch)
    );
}