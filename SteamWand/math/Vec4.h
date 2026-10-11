#pragma once
#include "math.h"

#include <iostream>

// 3d Vector library
struct vec4 {
    float x;
    float y;
    float z;
    float w;

    vec4();
    vec4(float _x, float _y, float _z, float _w);
    vec4(const vec4& v);
    vec4& operator=(const vec4& rhs);
    const vec4 operator-() const;

    const vec4 operator+(const vec4& rhs);

    vec4& operator+=(const vec4& rhs);
    const vec4 operator-(const vec4& rhs) const;

    vec4& operator-=(const vec4& rhs);
    vec4& operator*=(float scalar);
    const vec4 operator*(const float scalar);

    bool operator==(const vec4& rhs);
    bool operator!=(const vec4& rhs);

    float& operator[](const size_t i);
    const float& operator[](const size_t i) const;
};

inline vec4::vec4() {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    w = 0.0f;
}

inline vec4::vec4(float _x, float _y, float _z, float _w) {
    x = _x;
    y = _y;
    z = _z;
    w = _w;
}

inline vec4::vec4(const vec4& v) {
    x = v.x;
    y = v.y;
    z = v.z;
    w = v.w;
}

inline vec4& vec4::operator=(const vec4& rhs) {
    x = rhs.x;
    y = rhs.y;
    z = rhs.z;
    w = rhs.w;
    return *this;
}

inline const vec4 vec4::operator+(const vec4& rhs) {
    return vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
}

inline vec4& vec4::operator+=(const vec4& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    w += rhs.w;
    return *this;
}

inline const vec4 vec4::operator-() const {
    return vec4(-x, -y, -z, -w);
}

inline const vec4 vec4::operator-(const vec4& rhs) const {
    return vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
}

inline vec4& vec4::operator-=(const vec4& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    w -= rhs.w;
    return *this;
}

inline vec4& vec4::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    w *= scalar;
    return *this;
}

inline const vec4 vec4::operator*(const float scalar) {
    return vec4(x * scalar, y * scalar, z * scalar, w * scalar);
}

inline bool vec4::operator==(const vec4& rhs) {
    if (x != rhs.x)
        return false;
    if (y != rhs.y)
        return false;
    if (z != rhs.z)
        return false;
    if (w != rhs.w)
        return false;
    return true;
}

inline bool vec4::operator!=(const vec4& rhs) {
    if (x != rhs.x)
        return true;
    if (y != rhs.y)
        return true;
    if (z != rhs.z)
        return true;
    if (w != rhs.w)
        return true;
    return false;
}

inline float& vec4::operator[](const size_t i) {
    if (i == 0)
        return x;
    if (i == 1)
        return y;
    if (i == 2)
        return z;
    if (i == 3)
        return w;
}

inline const float& vec4::operator[](const size_t i) const {
    if (i == 0)
        return x;
    if (i == 1)
        return y;
    if (i == 2)
        return z;
    if (i == 3)
        return w;
}

inline float dot(const vec4& a, const vec4& b) {
    float num = 0;
    num += a.x * b.x;
    num += a.y * b.y;
    num += a.z * b.z;
    num += a.w * b.w;
    return num;
}

// aka magnitude of vector
inline float length(const vec4& v) {
    float num = 0;
    num = sqrt(pow(v.x, 2) + pow(v.y, 2) + pow(v.z, 2) + pow(v.w, 2));
    return num;
}

inline vec4 normalize(const vec4& v) {
    float len = length(v);
    if (len == 0)
        return vec4(0.0f, 0.0f, 0.0f, 0.0f);
    return vec4(v.x / len, v.y / len, v.z / len, v.w / len);
}
