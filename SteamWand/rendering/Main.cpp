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

_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    Renderer renderer;

    renderer.SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    auto cubeData = Shapes::MakeCube();
    GPUMesh cubeMesh = renderer.UploadMesh(cubeData);

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

        Update();
        renderer.Render(cubeMesh);
    }

    renderer.Shutdown();
    return 0;
}
