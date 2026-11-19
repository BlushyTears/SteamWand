#pragma once

#include <vector>
#include <cstdint>

#include <cmath>
#include "../math/Vec3.h"

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <unordered_map>

template<typename Vertex>
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

namespace Meshes {
    struct Vertex {
        vec3 position;
        float u;
        float v;
    };

    struct ObjPart {
        Mesh<Vertex> mesh;
        std::unordered_map<std::string, size_t> uniqueVertexMap;
    };

    inline std::unordered_map<std::string, ObjPart> LoadObjFile(const char* path) {
        std::unordered_map<std::string, ObjPart> buckets;

        std::ifstream file(path);

        std::vector<float> pos;
        std::vector<float> norm;
        std::vector<float> uv;

        std::string line;
        std::string currentMaterial;

        while (std::getline(file, line)) {
            if (line.empty())
                continue;

            std::istringstream lineStream(line);
            std::string token;
            lineStream >> token;

            if (token == "v") {
                float x, y, z;
                lineStream >> x >> y >> z;
                pos.push_back(x);
                pos.push_back(y);
                pos.push_back(z);
            }
            else if (token == "vn") {
                float x, y, z;
                lineStream >> x >> y >> z;
                norm.push_back(x);
                norm.push_back(y);
                norm.push_back(z);
            }
            else if (token == "vt") {
                float u, v;
                lineStream >> u >> v;
                uv.push_back(u);
                uv.push_back(v);
            }
            else if (token == "usemtl") {
                lineStream >> currentMaterial;

            }
            else if (token == "f") {
                ObjPart& bucket = buckets[currentMaterial];

                std::string line;
                std::getline(lineStream, line);

                std::istringstream ls(line);

                std::vector<size_t> face;
                std::string vertex;

                while (std::getline(ls, vertex, ' ')) {
                    if (vertex.empty())
                        continue;

                    int v = -1;
                    int t = -1;
                    int n = -1;

                    std::istringstream vs(vertex);
                    std::string part;

                    int i = 0;
                    while (std::getline(vs, part, '/')) {
                        if (!part.empty()) {
                            size_t idx = std::stoi(part) - 1;
                            if (i == 0)
                                v = idx;
                            else if (i == 1)
                                t = idx;
                            else if (i == 2)
                                n = idx;
                        }
                        i++;
                    }

                    float vx = pos[v * 3 + 0];
                    float vy = pos[v * 3 + 1];
                    float vz = pos[v * 3 + 2];

                    float tu = uv[t * 2 + 0];
                    float tv = 1.0f - uv[t * 2 + 1]; // needs to flip upside down

                    float nx = norm[n * 3 + 0];
                    float ny = norm[n * 3 + 1];
                    float nz = norm[n * 3 + 2];

                    size_t index;

                    auto it = bucket.uniqueVertexMap.find(vertex);

                    // duplicate vertex
                    if (it != bucket.uniqueVertexMap.end()) {
                        index = it->second;
                    }

                    // new vertex
                    else {
                        Vertex newV({ vx, vy, vz }, tu, tv);

                        index = bucket.mesh.vertices.size();
                        bucket.mesh.vertices.push_back(newV);

                        bucket.uniqueVertexMap[vertex] = index;
                    }

                    face.push_back(index);
                }

                for (int i = 1; i + 1 < face.size(); i++) {
                    bucket.mesh.indices.push_back(face[0]);
                    bucket.mesh.indices.push_back(face[i]);
                    bucket.mesh.indices.push_back(face[i + 1]);
                }
            }
        }

        file.close();
        return buckets;
    }

    inline std::unordered_map<std::string, std::string> LoadMtlFile(const char* path)
    {
        std::ifstream file(path);
        std::string line;

        std::unordered_map<std::string, std::string> texturePaths;
        std::string currentName;

        while (std::getline(file, line)) {
            if (line.empty())
                continue;

            std::istringstream lineStream(line);

            std::string token;
            lineStream >> token;

            if (token == "newmtl") {
                lineStream >> currentName;
            }
            else if (token == "map_Kd")
            {
                std::string texturePath;
                lineStream >> texturePath;
                texturePaths[currentName] = texturePath;
            }
        }

        return texturePaths;
    }

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

