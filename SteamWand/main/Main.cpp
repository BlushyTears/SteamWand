#include "../rendering/App.h"
#include "Misc.h"

static uint32_t spinX, spinY, spinZ;

void Update() {
    GetRenderObject(spinX).rotation.x += 15.0f * GetDeltaTime();
    GetRenderObject(spinY).rotation.y += 15.0f * GetDeltaTime();
    GetRenderObject(spinZ).rotation.z += 15.0f * GetDeltaTime();

    Freecam();
}

void Start() {
    // Reminder: Working directory is bin/x64/Debug/SteamWandMain
    uint32_t brick = CreateTexture(LoadImageFile("../../../../assets/wall-texture.png"));

    // Reminder: Working directory is bin/x64/Debug/SteamWandMain
    uint32_t checker = CreateTexture(LoadImageFile("../../../../assets/checker.png"));

    uint32_t sponzaTexture = CreateTexture(LoadImageFile("../../../../assets/checker.png"));

    uint32_t colored = CreateMaterial<ColoredShapes::Vertex>(L"ColoredShapes.hlsl");
    uint32_t textured = CreateMaterial<Meshes::Vertex>(L"Shapes.hlsl");

    uint32_t coloredCube = CreateMesh(ColoredShapes::MakeCube());
    Spawn({ .mesh = coloredCube, .position = vec3(-2, 2, 2), .material = colored });

    auto cubeData1 = Meshes::MakeCube();
    uint32_t cubeMesh1 = CreateMesh(cubeData1);
    spinX = Spawn({ .mesh = cubeMesh1, .position = vec3(1, 2, 1), .scale = vec3(5, 1, 1), .material = textured, .texture = brick });

    auto cubeData2 = Meshes::MakeCube();
    uint32_t cubeMesh2 = CreateMesh(cubeData2);
    spinY = Spawn({ .mesh = cubeMesh2, .position = vec3(4, 2, 2), .scale = vec3(1, 2, 1), .material = textured, .texture = checker});

    auto cubeData3 = ColoredShapes::MakeCube();
    uint32_t cubeMesh3 = CreateMesh(cubeData3);
    spinZ = Spawn({ .mesh = cubeMesh3, .position = vec3(-4, 2, 1), .material = colored, .texture = checker });

    auto cubeData4 = ColoredShapes::MakeCube();
    uint32_t cubeMesh4 = CreateMesh(cubeData4);
    Spawn({ .mesh = cubeMesh4, .position = vec3(-7, 2, 2), .material = colored, .texture = checker });

    auto prismTriangleData = Meshes::MakeTriangularPrism();
    uint32_t prismMesh = CreateMesh(prismTriangleData);
    Spawn({ .mesh = prismMesh, .position = vec3(1, -2, 2), .material = textured });

    auto prismTriangleData1 = ColoredShapes::MakeTriangularPrism();
    uint32_t prismMesh1 = CreateMesh(prismTriangleData1);
    Spawn({ .mesh = prismMesh1, .position = vec3(-4, -2, 2), .material = colored });

    auto planeData1 = Meshes::MakePlane();
    uint32_t planeMesh1 = CreateMesh(planeData1);
    Spawn({ .mesh = planeMesh1, .position = vec3(1, 0, 2), .scale = vec3(3, 1, 1), .material = textured });

    auto planeData = ColoredShapes::MakePlane();
    uint32_t planeMesh = CreateMesh(planeData);
    Spawn({ .mesh = planeMesh, .position = vec3(-4, 0, 2), .scale = vec3(5, 5, 2), .material = colored});

    auto planeData2 = ColoredShapes::MakePlane();
    uint32_t planeMesh2 = CreateMesh(planeData2);
    Spawn({ .mesh = planeMesh2, .position = vec3(-4, 0, 2), .material = colored });

    // Very temporary way of reading .obj files with textures (will be made a lot simpler)
    auto sponzaParts = Meshes::LoadObjFile("../../../../assets/sponza/sponza.obj");
    auto texturePaths = Meshes::LoadMtlFile("../../../../assets/sponza/sponza.mtl");

    for (auto& [name, bucket] : sponzaParts)
    {
        uint32_t texture = checker;
        if (texturePaths.contains(name))
        {
            std::string fullPath = "../../../../assets/sponza/" + texturePaths[name];
            texture = CreateTexture(LoadImageFile(fullPath.c_str()));
        }
        uint32_t cubeMesh0 = CreateMesh(bucket.mesh);
        Spawn({ .mesh = cubeMesh0, .position = vec3(1, -2, 1), .scale = vec3(0.01f, 0.01f, 0.01f), .material = textured, .texture = texture });
    }
}


