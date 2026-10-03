#pragma once

#include <iostream>
#include <cmath>

#include "vec3.h"
#include "vec4.h"
#include "math.h"

struct mat4 {
    vec4 vec[4];

    mat4(); // identity matrix
    mat4(const vec4& r0, const vec4& r1, const vec4& r2, const vec4& r3);
    mat4(const mat4& m); // if we wanna copy the vector

    mat4& operator=(const mat4& rhs);
    const mat4 operator*(const mat4& rhs) const;
    const vec4 operator*(const vec4& rhs) const;

    const bool operator==(const mat4& rhs);
    const bool operator!=(const mat4& rhs);

    vec4& operator[](uint32_t i);
    const vec4& operator[](uint32_t i) const;
};

mat4::mat4() {
    // Identity matrix
    vec[0] = vec4(1.0f, 0.0f, 0.0f, 0.0f);
    vec[1] = vec4(0.0f, 1.0f, 0.0f, 0.0f);
    vec[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
    vec[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

mat4::mat4(const vec4& r0, const vec4& r1, const vec4& r2, const vec4& r3) {
    vec[0] = r0;
    vec[1] = r1;
    vec[2] = r2;
    vec[3] = r3;
}

mat4::mat4(const mat4& m) {
    vec[0] = m.vec[0];
    vec[1] = m.vec[1];
    vec[2] = m.vec[2];
    vec[3] = m.vec[3];
}

mat4& mat4::operator=(const mat4& rhs) {
    if (this != &rhs) {
        vec[0] = rhs.vec[0];
        vec[1] = rhs.vec[1];
        vec[2] = rhs.vec[2];
        vec[3] = rhs.vec[3];
    }
    return *this;
}

const mat4 mat4::operator*(const mat4& rhs) const {
    mat4 result;
    for (int i = 0; i < 4; ++i) {
        result.vec[i] = vec4(
            vec[0].x * rhs.vec[i].x + vec[1].x * rhs.vec[i].y + vec[2].x * rhs.vec[i].z + vec[3].x * rhs.vec[i].w,
            vec[0].y * rhs.vec[i].x + vec[1].y * rhs.vec[i].y + vec[2].y * rhs.vec[i].z + vec[3].y * rhs.vec[i].w,
            vec[0].z * rhs.vec[i].x + vec[1].z * rhs.vec[i].y + vec[2].z * rhs.vec[i].z + vec[3].z * rhs.vec[i].w,
            vec[0].w * rhs.vec[i].x + vec[1].w * rhs.vec[i].y + vec[2].w * rhs.vec[i].z + vec[3].w * rhs.vec[i].w
        );
    }
    return result;
}

const vec4 mat4::operator*(const vec4& rhs) const {
    return vec4(
        vec[0].x * rhs.x + vec[1].x * rhs.y + vec[2].x * rhs.z + vec[3].x * rhs.w,
        vec[0].y * rhs.x + vec[1].y * rhs.y + vec[2].y * rhs.z + vec[3].y * rhs.w,
        vec[0].z * rhs.x + vec[1].z * rhs.y + vec[2].z * rhs.z + vec[3].z * rhs.w,
        vec[0].w * rhs.x + vec[1].w * rhs.y + vec[2].w * rhs.z + vec[3].w * rhs.w
    );
}

const bool mat4::operator==(const mat4& rhs) {
    return vec[0] == rhs.vec[0] &&
        vec[1] == rhs.vec[1] &&
        vec[2] == rhs.vec[2] &&
        vec[3] == rhs.vec[3];
}

const bool mat4::operator!=(const mat4& rhs) {
    return !(*this == rhs);
}

vec4& mat4::operator[](uint32_t i) {
    return vec[i];
}

const vec4& mat4::operator[](uint32_t i) const {
    return vec[i];
}

// A non-recursive determinant function
float determinant(const mat4& m) {
    float cofactor00 = m.vec[1].y * (m.vec[2].z * m.vec[3].w - m.vec[2].w * m.vec[3].z) -
        m.vec[1].z * (m.vec[2].y * m.vec[3].w - m.vec[2].w * m.vec[3].y) +
        m.vec[1].w * (m.vec[2].y * m.vec[3].z - m.vec[2].z * m.vec[3].y);

    float cofactor01 = m.vec[1].x * (m.vec[2].z * m.vec[3].w - m.vec[2].w * m.vec[3].z) -
        m.vec[1].z * (m.vec[2].x * m.vec[3].w - m.vec[2].w * m.vec[3].x) +
        m.vec[1].w * (m.vec[2].x * m.vec[3].z - m.vec[2].z * m.vec[3].x);

    float cofactor02 = m.vec[1].x * (m.vec[2].y * m.vec[3].w - m.vec[2].w * m.vec[3].y) -
        m.vec[1].y * (m.vec[2].x * m.vec[3].w - m.vec[2].w * m.vec[3].x) +
        m.vec[1].w * (m.vec[2].x * m.vec[3].y - m.vec[2].y * m.vec[3].x);

    float cofactor03 = m.vec[1].x * (m.vec[2].y * m.vec[3].z - m.vec[2].z * m.vec[3].y) -
        m.vec[1].y * (m.vec[2].x * m.vec[3].z - m.vec[2].z * m.vec[3].x) +
        m.vec[1].z * (m.vec[2].x * m.vec[3].y - m.vec[2].y * m.vec[3].x);

    float det = m.vec[0].x * cofactor00 -
        m.vec[0].y * cofactor01 +
        m.vec[0].z * cofactor02 -
        m.vec[0].w * cofactor03;

    return det;
}

mat4 transpose(const mat4& m) {
    vec4 v1 = vec4(m.vec[0].x, m.vec[1].x, m.vec[2].x, m.vec[3].x);
    vec4 v2 = vec4(m.vec[0].y, m.vec[1].y, m.vec[2].y, m.vec[3].y);
    vec4 v3 = vec4(m.vec[0].z, m.vec[1].z, m.vec[2].z, m.vec[3].z);
    vec4 v4 = vec4(m.vec[0].w, m.vec[1].w, m.vec[2].w, m.vec[3].w);
    return mat4(v1, v2, v3, v4);
}

mat4 cofactorMatrix(const mat4& mat) {
    mat4 cofactor;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            float minor[3][3];
            int row = 0;
            int col = 0;
            for (int r = 0; r < 4; r++) {
                if (r == i)
                    continue;
                col = 0;
                for (int c = 0; c < 4; c++) {
                    if (c == j)
                        continue;
                    minor[row][col] = mat[r][c];
                    col++;
                }
                row++;
            }
            cofactor[i][j] = ((i + j) % 2 == 0 ? 1.0f : -1.0f) *
                (minor[0][0] * (minor[1][1] * minor[2][2] - minor[1][2] * minor[2][1]) -
                    minor[0][1] * (minor[1][0] * minor[2][2] - minor[1][2] * minor[2][0]) +
                    minor[0][2] * (minor[1][0] * minor[2][1] - minor[1][1] * minor[2][0]));
        }
    }
    return cofactor;
}

mat4 inverse(const mat4& m) {
    float det = determinant(m);
    // identiy upon failure
    if (det == 0) {
        return mat4();
    }

    mat4 adjugate = transpose(cofactorMatrix(m));
    mat4 inv;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            inv[i][j] = adjugate[i][j] / det;
        }
    }
    return inv;
}

// Rotate about the x-axis
mat4 rotationx(float rad) {
    mat4 m;
    m.vec[0] = vec4(1.0f, 0.0f, 0.0f, 0.0f);
    m.vec[1] = vec4(0.0f, cos(rad), -sin(rad), 0.0f);
    m.vec[2] = vec4(0.0f, sin(rad), cos(rad), 0.0f);
    m.vec[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return transpose(m);
}

// Rotate about the y-axis
mat4 rotationy(float rad) {
    mat4 m;
    m.vec[0] = vec4(cos(rad), 0.0f, sin(rad), 0.0f);
    m.vec[1] = vec4(0.0f, 1.0f, 0.0f, 0.0f);
    m.vec[2] = vec4(-sin(rad), 0.0f, cos(rad), 0.0f);
    m.vec[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return transpose(m);
}

// Rotate about the z-axis
mat4 rotationz(float rad) {
    mat4 m;
    m.vec[0] = vec4(cos(rad), -sin(rad), 0.0f, 0.0f);
    m.vec[1] = vec4(sin(rad), cos(rad), 0.0f, 0.0f);
    m.vec[2] = vec4(0.0f, 0.0f, 1.0f, 0.0f);
    m.vec[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    return transpose(m);
}

// Rotate about any axis using the roudriges formula
// Note: w is 1, so we allow for translation as well
mat4 rotationaxis(const vec3& v, const float rad) {
    vec3 axis = normalize(v);

    float cosTheta = cos(rad);
    float sinTheta = sin(rad);
    float oneMinusCosTheta = 1.0f - cosTheta;

    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    mat4 result;
    result[0] = vec4(
        cosTheta + x * x * oneMinusCosTheta,
        x * y * oneMinusCosTheta - z * sinTheta,
        x * z * oneMinusCosTheta + y * sinTheta,
        0.0f
    );
    result[1] = vec4(
        y * x * oneMinusCosTheta + z * sinTheta,
        cosTheta + y * y * oneMinusCosTheta,
        y * z * oneMinusCosTheta - x * sinTheta,
        0.0f
    );
    result[2] = vec4(
        z * x * oneMinusCosTheta - y * sinTheta,
        z * y * oneMinusCosTheta + x * sinTheta,
        cosTheta + z * z * oneMinusCosTheta,
        0.0f
    );
    result[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    return transpose(result);
}

mat4 perspective(float fovy, float aspect, float nearPlane, float farPlane) {
    float tan_half_fovy = tan(fovy / 2.0f);

    // Initialize as an identity matrix
    mat4 result;

    result[0] = vec4(1.0f / (aspect * tan_half_fovy), 0.0f, 0.0f, 0.0f);
    result[1] = vec4(0.0f, 1.0f / tan_half_fovy, 0.0f, 0.0f);
    result[2] = vec4(0.0f, 0.0f, (farPlane + nearPlane) / (farPlane - nearPlane), 2.0f * farPlane * nearPlane / (farPlane - nearPlane));
    result[3] = vec4(0.0f, 0.0f, -1.0f, 0.0f);

    return result;
}

mat4 lookat(const vec3& eye, const vec3& at, const vec3& up) {
    vec3 forward = normalize(eye - at);
    vec3 right = normalize(up.cross(forward));
    vec3 up_corrected = forward.cross(right);

    mat4 result;

    result[0] = vec4(right.x, right.y, right.z, 0.0f);
    result[1] = vec4(up_corrected.x, up_corrected.y, up_corrected.z, 0.0f);
    result[2] = vec4(-forward.x, -forward.y, -forward.z, 0.0f);
    result[3] = vec4(-right.dot(eye), -up_corrected.dot(eye), forward.dot(eye), 1.0f);

    return result;
}

