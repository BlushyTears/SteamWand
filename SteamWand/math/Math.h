#pragma once

#include <cmath>
#include <numbers>

// This might work better if it's a macro or something else than a function
inline float radians(float degrees) {
    return degrees * (std::numbers::pi_v<float> / 180.0f);
}
