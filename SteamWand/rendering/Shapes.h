#pragma once

#include <vector>
#include <cstdint>

#include <cmath>
#include "../math/Vec3.h"

namespace Shapes {
    template<typename Vertex>
    struct Mesh {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
    };

    inline Mesh<vec3>MakeCube() {
        Mesh<vec3> mesh;

        mesh.vertices = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
        };

        mesh.indices = {
            0, 1, 2,  0, 2, 3,
            4, 6, 5,  4, 7, 6,
            0, 3, 7,  0, 7, 4,
            1, 5, 6,  1, 6, 2,
            3, 2, 6,  3, 6, 7,
            0, 4, 5,  0, 5, 1
        };

        return mesh;
    }

    inline Mesh<vec3>MakeSomething() {
        Mesh<vec3> mesh;

        mesh.vertices = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        };

        mesh.indices = {
            0, 1, 2,  0, 2, 3,
            4, 6, 5,  4, 7, 6,
        };

        return mesh;
    }
}

