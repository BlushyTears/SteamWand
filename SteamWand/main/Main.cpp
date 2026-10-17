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
    g_CubeAngle += GetDeltaTime();
    MoveAround();
}

void Start(Renderer& renderer) {
    //auto cubeData = Shapes::MakeCube();
    //GPUMesh cubeMesh = renderer.UploadMesh(cubeData);
    //renderer.meshes.push_back({ cubeMesh, vec3(1, 2, 1) });

    //auto cubeData2 = Shapes::MakeCube();
    //GPUMesh cubeMesh2 = renderer.UploadMesh(cubeData2);
    //renderer.meshes.push_back({ cubeMesh2, vec3(4, 2, 2) });

    //auto prismTriangleData = Shapes::MakeTriangularPrism();
    //GPUMesh prismMesh = renderer.UploadMesh(prismTriangleData);
    //renderer.meshes.push_back({ prismMesh, vec3(1, -2, 2) });

    //auto planeData = Shapes::MakePlane();
    //GPUMesh planeMesh = renderer.UploadMesh(planeData);
    //renderer.meshes.push_back({ planeMesh, vec3(1, 0, 2) });

    auto cubeData = ColoredShapes::MakeCube();
    GPUMesh cubeMesh = renderer.UploadMesh(cubeData);
    renderer.meshes.push_back({ cubeMesh, vec3(1, 2, 1) });

    auto cubeData2 = ColoredShapes::MakeCube();
    GPUMesh cubeMesh2 = renderer.UploadMesh(cubeData2);
    renderer.meshes.push_back({ cubeMesh2, vec3(4, 2, 2) });

    auto prismTriangleData = ColoredShapes::MakeTriangularPrism();
    GPUMesh prismMesh = renderer.UploadMesh(prismTriangleData);
    renderer.meshes.push_back({ prismMesh, vec3(1, -2, 2) });

    auto planeData = ColoredShapes::MakePlane();
    GPUMesh planeMesh = renderer.UploadMesh(planeData);
    renderer.meshes.push_back({ planeMesh, vec3(1, 0, 2) });

    // Reminder: Working directory is bin/x64/Debug/SteamWandMain
    //renderer.UploadTexture(LoadImageFile("../../../../assets/checker.png"));
}
