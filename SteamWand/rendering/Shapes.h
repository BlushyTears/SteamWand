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

    struct Cube {
        float x;
        float y;
        float size;
    };

    inline std::vector<VerticalLine> g_Lines;
    inline std::vector<Triangle> g_Triangles;
    inline std::vector<Quad> g_Quads;
    inline std::vector<Cube> g_Cubes;

    void AddVerticalLine(float x, float y, float size) {
        g_Lines.push_back({ x, y, size });
    }

    void AddQuad(float x, float y, float size) {
        g_Quads.push_back({ x, y, size });
    }

    void AddTriangle(float x, float y, float size) {
        g_Triangles.push_back({ x, y, size });
    }
    void AddCube(float x, float y, float size) {
        g_Cubes.push_back({ x, y, size });
    }
}

//MeshBuilder& MeshBuilder::BuildCube(float width, float height, float depth) {
//    float w2 = width * 0.5f;
//    float h2 = height * 0.5f;
//    float d2 = depth * 0.5f;
//
//    uint currBaseIndex;
//
//    currBaseIndex = vertices.size();
//    BuildVertex(-w2, -h2, d2, 0, 0);
//    BuildVertex(w2, -h2, d2, 1, 0);
//    BuildVertex(w2, h2, d2, 1, 1);
//    BuildVertex(-w2, h2, d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    currBaseIndex = vertices.size();
//    BuildVertex(w2, -h2, -d2, 0, 0);
//    BuildVertex(-w2, -h2, -d2, 1, 0);
//    BuildVertex(-w2, h2, -d2, 1, 1);
//    BuildVertex(w2, h2, -d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    currBaseIndex = vertices.size();
//    BuildVertex(-w2, -h2, -d2, 0, 0);
//    BuildVertex(-w2, -h2, d2, 1, 0);
//    BuildVertex(-w2, h2, d2, 1, 1);
//    BuildVertex(-w2, h2, -d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    currBaseIndex = vertices.size();
//    BuildVertex(w2, -h2, d2, 0, 0);
//    BuildVertex(w2, -h2, -d2, 1, 0);
//    BuildVertex(w2, h2, -d2, 1, 1);
//    BuildVertex(w2, h2, d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    currBaseIndex = vertices.size();
//    BuildVertex(-w2, h2, d2, 0, 0);
//    BuildVertex(w2, h2, d2, 1, 0);
//    BuildVertex(w2, h2, -d2, 1, 1);
//    BuildVertex(-w2, h2, -d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    currBaseIndex = vertices.size();
//    BuildVertex(-w2, -h2, -d2, 0, 0);
//    BuildVertex(w2, -h2, -d2, 1, 0);
//    BuildVertex(w2, -h2, d2, 1, 1);
//    BuildVertex(-w2, -h2, d2, 0, 1);
//    ConstructTriangle(currBaseIndex + 0, currBaseIndex + 1, currBaseIndex + 2);
//    ConstructTriangle(currBaseIndex + 2, currBaseIndex + 3, currBaseIndex + 0);
//
//    return *this;
//}