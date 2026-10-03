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

    SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

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

        if (msg.message == WM_QUIT) {
            break;
        }

        if (inputHandler.isKeyPressed(Button::Escape)) {
            break;
        }

        if (inputHandler.isKeyPressed(Button::V)) {
            g_VSync = !g_VSync;
        }

        if (inputHandler.isKeyPressed(Button::F11) ||
            (inputHandler.isKeyDown(Button::Alt) && inputHandler.isKeyPressed(Button::Enter))) {
            SetFullScreen(!g_FullScreen);
        }

        if (inputHandler.isKeyPressed(Button::MouseLeft)) {
            float x = 2.0f * inputHandler.mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * inputHandler.mouseY / g_ClientHeight;

            Shapes::AddTriangle(x, y, 0.2f);
        }

        if (inputHandler.isKeyPressed(Button::MouseRight)) {
            float x = 2.0f * inputHandler.mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * inputHandler.mouseY / g_ClientHeight;

            Shapes::AddVerticalLine(x - 0.001f, y, 0.2f);
            Shapes::AddVerticalLine(x, y, 0.2f);
            Shapes::AddVerticalLine(x + 0.001f, y, 0.2f);
        }

        if (inputHandler.isMouseScrolling()) {
            float x = 2.0f * inputHandler.mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * inputHandler.mouseY / g_ClientHeight;

            Shapes::AddQuad(x, y, 0.2f);
        }

        Update();
        Render();
    }

    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
    ::CloseHandle(g_FenceEvent);
    return 0;
}
