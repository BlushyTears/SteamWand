#pragma once
#include "math.h"

struct vec3 {
    float x;
    float y;
    float z;

    vec3();
    vec3(float _x, float _y, float _z);
    vec3(const vec3& v);
    vec3& operator=(const vec3& rhs);
    const vec3 operator-();

    const vec3 operator+(const vec3& rhs);

    vec3& operator+=(const vec3& rhs);
    const vec3 operator-(const vec3& rhs) const;

    vec3& operator-=(const vec3& rhs);
    vec3& operator*=(const vec3& rhs);
    vec3& operator*=(const float scalar);
    const vec3 operator*(const float scalar);

    bool operator==(const vec3& rhs);
    bool operator!=(const vec3& rhs);
    float& operator[](const size_t i);
    const float& operator[](const size_t i) const;

    float dot(const vec3& rhs) const;
    vec3 cross(const vec3& rhs) const;
};

inline vec3::vec3() {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
}

inline vec3::vec3(float _x, float _y, float _z) {
    x = _x;
    y = _y;
    z = _z;
}

inline vec3::vec3(const vec3& v) {
    x = v.x;
    y = v.y;
    z = v.z;
}

inline vec3& vec3::operator=(const vec3& rhs) {
    x = rhs.x;
    y = rhs.y;
    z = rhs.z;
    return *this;
}

inline const vec3 vec3::operator-() {
    return vec3(-x, -y, -z);
}

inline const vec3 vec3::operator-(const vec3& rhs) const {
    return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
}

inline const vec3 vec3::operator+(const vec3& rhs) {
    return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
}

inline vec3& vec3::operator+=(const vec3& rhs) {
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
    return *this;
}

inline vec3& vec3::operator-=(const vec3& rhs) {
    x -= rhs.x;
    y -= rhs.y;
    z -= rhs.z;
    return *this;
}

inline vec3& vec3::operator*=(const vec3& rhs) {
    x *= rhs.x;
    y *= rhs.y;
    z *= rhs.z;
    return *this;
}

inline vec3& vec3::operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
}

inline const vec3 vec3::operator*(const float scalar) {
    return vec3::vec3(x * scalar, y * scalar, z * scalar);
}

inline bool vec3::operator==(const vec3& rhs) {
    if (x != rhs.x)
        return false;
    if (y != rhs.y)
        return false;
    if (z != rhs.z)
        return false;
    return true;
}

inline bool vec3::operator!=(const vec3& rhs) {
    if (x != rhs.x)
        return true;
    if (y != rhs.y)
        return true;
    if (z != rhs.z)
        return true;
    return false;
}

inline float& vec3::operator[](const size_t i) {
    if (i == 0)
        return x;
    if (i == 1)
        return y;
    if (i == 2)
        return z;
}

inline const float& vec3::operator[](const size_t i) const {
    if (i == 0)
        return x;
    if (i == 1)
        return y;
    if (i == 2)
        return z;
}

// Dot product of two vec3s
inline float vec3::dot(const vec3& rhs) const {
    return x * rhs.x + y * rhs.y + z * rhs.z;
}

inline vec3 vec3::cross(const vec3& rhs) const {
    return vec3(
        y * rhs.z - z * rhs.y,
        z * rhs.x - x * rhs.z,
        x * rhs.y - y * rhs.x
    );
}

// Dot product for external use
inline float dot(const vec3& a, const vec3& b) {
    float num = 0;
    num += a.x * b.x;
    num += a.y * b.y;
    num += a.z * b.z;
    return num;
}

// aka magnitude of vector
inline float length(const vec3& v) {
    float num = 0;
    num = sqrt(pow(v.x, 2) + pow(v.y, 2) + pow(v.z, 2));
    return num;
}

inline vec3 cross(const vec3& a, const vec3& b) {
    return vec3(
        (a.y * b.z) - (a.z * b.y),
        (a.z * b.x) - (a.x * b.z),
        (a.x * b.y) - (a.y * b.x));
}

inline vec3 normalize(const vec3& v) {
    return vec3(v.x / length(v), v.y / length(v), v.z / length(v));
}


