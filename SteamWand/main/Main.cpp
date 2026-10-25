#include "../rendering/App.h"

void MoveAround() {
    if (inputHandler.isKeyDown(Button::MouseRight) && !inputHandler.isKeyPressed(Button::MouseRight)) {
        float sensitivity = 0.003f;

        g_CameraYaw += (inputHandler.mouseX - inputHandler.previousMouseX) * sensitivity;
        g_CameraPitch -= (inputHandler.mouseY - inputHandler.previousMouseY) * sensitivity;

        g_CameraPitch = std::clamp(g_CameraPitch, -1.5f, 1.5f);
    }

    if (inputHandler.isKeyDown(Button::MouseRight)) {
        float step = 3.0f * GetDeltaTime();

        vec3 forward = GetCameraForward();
        vec3 right(std::cos(g_CameraYaw), 0.0f, -std::sin(g_CameraYaw));

        vec3 movement(0.0f, 0.0f, 0.0f);

        if (inputHandler.isKeyDown(Button::W)) movement += forward;
        if (inputHandler.isKeyDown(Button::S)) movement -= forward;
        if (inputHandler.isKeyDown(Button::A)) movement -= right;
        if (inputHandler.isKeyDown(Button::D)) movement += right;

        if (movement.dot(movement) > 0.0f)
            g_CameraPosition += normalize(movement) * step;
    }
}

void Update(Renderer& renderer) {
    renderer.meshes[0].rotation.y += 10.0f * GetDeltaTime();
    renderer.meshes[1].rotation.x += 15.0f * GetDeltaTime();
    renderer.meshes[2].rotation.z += 15.0f * GetDeltaTime();
    MoveAround();
}

void Start(Renderer& renderer) {

    // Reminder: Working directory is bin/x64/Debug/SteamWandMain
    uint32_t brick = renderer.UploadTexture(LoadImageFile("../../../../assets/wall-texture.png"));

    // Reminder: Working directory is bin/x64/Debug/SteamWandMain
    uint32_t checker = renderer.UploadTexture(LoadImageFile("../../../../assets/checker.png"));

    uint32_t colored = renderer.CreateMaterial<ColoredShapes::Vertex>(L"ColoredShapes.hlsl");

    uint32_t textured = renderer.CreateMaterial<Shapes::Vertex>(L"Shapes.hlsl");

    GPUMesh coloredCube = renderer.UploadMesh(ColoredShapes::MakeCube());
    renderer.meshes.push_back({ .mesh = coloredCube, .position = vec3(-2, 2, 2), .material = colored });

    auto cubeData1 = Shapes::MakeCube();
    GPUMesh cubeMesh1 = renderer.UploadMesh(cubeData1);
    renderer.meshes.push_back({ .mesh = cubeMesh1, .position = vec3(1, 2, 1), .scale = vec3(5, 1, 1), .material = textured, .texture = brick });

    auto cubeData2 = Shapes::MakeCube();
    GPUMesh cubeMesh2 = renderer.UploadMesh(cubeData2);
    renderer.meshes.push_back({ .mesh = cubeMesh2, .position = vec3(4, 2, 2), .scale = vec3(1, 2, 1), .material = textured, .texture = checker});

    auto cubeData3 = ColoredShapes::MakeCube();
    GPUMesh cubeMesh3 = renderer.UploadMesh(cubeData3);
    renderer.meshes.push_back({ .mesh = cubeMesh3, .position = vec3(-4, 2, 1), .material = colored, .texture = checker });

    auto cubeData4 = ColoredShapes::MakeCube();
    GPUMesh cubeMesh4 = renderer.UploadMesh(cubeData4);
    renderer.meshes.push_back({ .mesh = cubeMesh4, .position = vec3(-7, 2, 2), .material = colored, .texture = checker });

    auto prismTriangleData = Shapes::MakeTriangularPrism();
    GPUMesh prismMesh = renderer.UploadMesh(prismTriangleData);
    renderer.meshes.push_back({ .mesh = prismMesh, .position = vec3(1, -2, 2), .material = textured });

    auto prismTriangleData1 = ColoredShapes::MakeTriangularPrism();
    GPUMesh prismMesh1 = renderer.UploadMesh(prismTriangleData1);
    renderer.meshes.push_back({ .mesh = prismMesh1, .position = vec3(-4, -2, 2), .material = colored });

    auto planeData1 = Shapes::MakePlane();
    GPUMesh planeMesh1 = renderer.UploadMesh(planeData1);
    renderer.meshes.push_back({ .mesh = planeMesh1, .position = vec3(1, 0, 2), .scale = vec3(3, 1, 1), .material = textured });

    auto planeData = ColoredShapes::MakePlane();
    GPUMesh planeMesh = renderer.UploadMesh(planeData);
    renderer.meshes.push_back({ .mesh = planeMesh, .position = vec3(-4, 0, 2), .material = colored });


}
