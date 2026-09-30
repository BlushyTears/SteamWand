#pragma once

#include <vector>

namespace Shapes {
    struct Triangle {
        float x;
        float y;
        float size;
    };

    struct Quad {
        float x;
        float y;
        float size;
    };

    struct VerticalLine {
        float x;
        float y;
        float size;
    };

    inline std::vector<VerticalLine> g_Lines;
    inline std::vector<Triangle> g_Triangles;
    inline std::vector<Quad> g_Quads;

    void AddVerticalLine(float x, float y, float size) {
        g_Lines.push_back({ x, y, size });
    }

    void AddQuad(float x, float y, float size) {
        g_Quads.push_back({ x, y, size });
    }

    void AddTriangle(float x, float y, float size) {
        g_Triangles.push_back({ x, y, size });
    }
}