#include <iostream>
#include <chrono>
#include <algorithm>
#include <cassert>
#include <vector>
#include <windowsx.h>

#include <Windows.h>
#include <shellapi.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include <wrl.h>

#include "d3dx12.h"

#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "Renderer.h"
#include "Window.h"
#include "Input.h"

using namespace Microsoft::WRL;

void MoveAround(Renderer renderer) {
    g_CubeAngle += std::chrono::duration<float>(renderer.deltaTime).count();

    if (inputHandler.isKeyDown(Button::MouseRight)) {
        float step = 3.0f * std::chrono::duration<float>(renderer.deltaTime).count();

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

_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    Renderer renderer;

    renderer.SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    auto cubeData = Shapes::MakeCube();
    GPUMesh cubeMesh = renderer.UploadMesh(cubeData);
    renderer.meshes.push_back({ cubeMesh, vec3(1, 2, 1) });

    auto cubeData2 = Shapes::MakeCube();
    GPUMesh cubeMesh2 = renderer.UploadMesh(cubeData2);
    renderer.meshes.push_back({ cubeMesh2, vec3(4, 2, 2) });

    auto somethingData = Shapes::MakeSomething();
    GPUMesh somethingMesh = renderer.UploadMesh(somethingData);
    renderer.meshes.push_back({ somethingMesh, vec3(-1, -1, 1) });

    MSG msg = {};

    while (msg.message != WM_QUIT) {
        inputHandler.BeginFrame();

        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (inputHandler.isKeyDown(Button::MouseRight) && !inputHandler.isKeyPressed(Button::MouseRight)) {
            float sensitivity = 0.003f;

            g_CameraYaw += (inputHandler.mouseX - inputHandler.previousMouseX) * sensitivity;
            g_CameraPitch -= (inputHandler.mouseY - inputHandler.previousMouseY) * sensitivity;

            g_CameraPitch = std::clamp(g_CameraPitch, -1.5f, 1.5f);
        }

        if (msg.message == WM_QUIT || inputHandler.isKeyPressed(Button::Escape)) {
            break;
        }

        if (inputHandler.isKeyPressed(Button::F2)) {
            renderer.g_Wireframe = !renderer.g_Wireframe;
        }

        if (inputHandler.isKeyPressed(Button::V)) {
            renderer.g_VSync = !renderer.g_VSync;
        }

        if (inputHandler.isKeyPressed(Button::F11) ||
            (inputHandler.isKeyDown(Button::Alt) && inputHandler.isKeyPressed(Button::Enter))) {
            SetFullScreen(!g_FullScreen);
        }

        if (inputHandler.isKeyPressed(Button::MouseLeft)) {
        }

        if (inputHandler.isMouseScrolling()) {
        }

        MoveAround(renderer);
        renderer.Update();
        renderer.Render();
    }

    renderer.Shutdown();
    return 0;
}
