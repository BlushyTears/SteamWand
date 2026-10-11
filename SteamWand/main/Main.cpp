#include "../rendering/App.h"





void Start(Renderer& renderer) {
    auto cubeData = Shapes::MakeCube();
    GPUMesh cubeMesh = renderer.UploadMesh(cubeData);
    renderer.meshes.push_back({ cubeMesh, vec3(1, 2, 1) });

    auto cubeData2 = Shapes::MakeCube();
    GPUMesh cubeMesh2 = renderer.UploadMesh(cubeData2);
    renderer.meshes.push_back({ cubeMesh2, vec3(4, 2, 2) });

    auto prismTriangleData = Shapes::MakeTriangularPrism();
    GPUMesh prismMesh = renderer.UploadMesh(prismTriangleData);
    renderer.meshes.push_back({ prismMesh, vec3(1, -2, 2) });

    //auto somethingData = Shapes::MakeSomething();
    //GPUMesh somethingMesh = renderer.UploadMesh(somethingData);
    //renderer.meshes.push_back({ somethingMesh, vec3(-1, -1, 1) });

    // Working directory is bin/x64/Debug/SteamWandMain, assets sits next to the .sln
    renderer.UploadTexture(LoadImageFile("../../../../assets/checker.png"));
}

void Update(Renderer& renderer, float deltaTime) {
    g_CubeAngle += deltaTime;
}
