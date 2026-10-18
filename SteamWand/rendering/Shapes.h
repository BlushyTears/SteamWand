#pragma once

#include <vector>
#include <cstdint>

#include <cmath>
#include "../math/Vec3.h"

template<typename Vertex>
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

namespace Shapes {
    struct Vertex {
        vec3 position;
        float u;
        float v;
    };

    inline Mesh<Vertex> MakeCube() {
        Mesh<Vertex> mesh;

        const vec3 corners[8] = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            {-0.5f, -0.5f,  0.5f},
            { 0.5f, -0.5f,  0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f}
        };

        const uint32_t faces[6][4] = {
            {3, 2, 1, 0}, // back   (-Z)
            {6, 7, 4, 5}, // front  (+Z)
            {2, 6, 5, 1}, // right  (+X)
            {7, 3, 0, 4}, // left   (-X)
            {7, 6, 2, 3}, // top    (+Y)
            {0, 1, 5, 4}, // bottom (-Y)
        };

        const float uvs[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

        for (uint32_t f = 0; f < 6; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], uvs[c][0], uvs[c][1] });
            }

            // Clockwise from outside = front face in DX12
            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }

    inline Mesh<Vertex> MakeTriangularPrism() {
        Mesh<Vertex> mesh;

        const vec3 corners[8] = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            { 0.0f,   0.5f, 0.5f},
            { 0.0f,  -0.5f, 0.5f},
        };

        // It might appear as wasteful for top and bottom to have two extra triangles
        // but unless you spawn 10 million of these or so, it won't be an issue
        const uint32_t faces[6][4] = {
            {3, 2, 1, 0}, // back side
            {4, 5, 1, 2}, // right side
            {4, 3, 0, 5}, // left side
            {4, 2, 3, 4}, // top side
            {5, 0, 1, 5}, // bottom side
        };

        const float uvs[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

        for (uint32_t f = 0; f < 5; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], uvs[c][0], uvs[c][1] });
            }

            // Clockwise from outside = front face in DX12
            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }

    inline Mesh<Vertex> MakePlane() {
        Mesh<Vertex> mesh;

        const vec3 corners[4] = {
            {-0.5f,  0.0f, 0.5f},
            {-0.5f, 0.0f, -0.5f},
            { 0.5f,  0.0f, 0.5f},
            { 0.5f, 0.0f, -0.5f},
        };

        const uint32_t faces[2][4] = {
            { 0, 2, 3, 1 },
            { 1, 3, 2, 0 },
        };

        const float uvs[4][2] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };

        for (uint32_t f = 0; f < 2; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], uvs[c][0], uvs[c][1] });
            }

            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }
}

namespace ColoredShapes {
    struct Vertex {
        vec3 position;
        float r;
        float g;
        float b;
    };

    inline Mesh<Vertex> MakeCube(float r = 0.9f, float g = 0.45f, float b = 0.2f) {
        Mesh<Vertex> mesh;

        const vec3 corners[8] = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            {-0.5f, -0.5f,  0.5f},
            { 0.5f, -0.5f,  0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f}
        };

        const uint32_t faces[6][4] = {
            {3, 2, 1, 0}, // back   (-Z)
            {6, 7, 4, 5}, // front  (+Z)
            {2, 6, 5, 1}, // right  (+X)
            {7, 3, 0, 4}, // left   (-X)
            {7, 6, 2, 3}, // top    (+Y)
            {0, 1, 5, 4}, // bottom (-Y)
        };

        for (uint32_t f = 0; f < 6; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], r, g, b });
            }

            // Clockwise from outside = front face in DX12
            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }

    inline Mesh<Vertex> MakeTriangularPrism(float r = 0.25f, float g = 0.75f, float b = 0.4f) {
        Mesh<Vertex> mesh;

        const vec3 corners[8] = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            { 0.0f,   0.5f, 0.5f},
            { 0.0f,  -0.5f, 0.5f},
        };

        // It might appear as wasteful for top and bottom to have two extra triangles
        // but unless you spawn 10 million of these or so, it won't be an issue
        const uint32_t faces[6][4] = {
            {3, 2, 1, 0}, // back side
            {4, 5, 1, 2}, // right side
            {4, 3, 0, 5}, // left side
            {4, 2, 3, 4}, // top side
            {5, 0, 1, 5}, // bottom side
        };

        for (uint32_t f = 0; f < 5; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], r, g, b });
            }

            // Clockwise from outside = front face in DX12
            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }

    inline Mesh<Vertex> MakePlane(float r = 0.3f, float g = 0.4f, float b = 0.85f) {
        Mesh<Vertex> mesh;

        const vec3 corners[4] = {
            {-0.5f,  0.0f, 0.5f},
            {-0.5f, 0.0f, -0.5f},
            { 0.5f,  0.0f, 0.5f},
            { 0.5f, 0.0f, -0.5f},
        };

        const uint32_t faces[2][4] = {
            { 0, 2, 3, 1 },
            { 1, 3, 2, 0 },
        };

        for (uint32_t f = 0; f < 2; f++) {
            for (uint32_t c = 0; c < 4; c++) {
                mesh.vertices.push_back({ corners[faces[f][c]], r, g, b });
            }

            uint32_t b = f * 4;
            mesh.indices.insert(mesh.indices.end(), { b, b + 1, b + 2,  b, b + 2, b + 3 });
        }

        return mesh;
    }
}

