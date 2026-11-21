#include "App.h"
#include <Windows.h>

using namespace Microsoft::WRL;

static std::chrono::duration<float> deltaTime{};
static Renderer renderer;

uint32_t CreateTexture(const Image& image)
{
    return renderer.UploadTexture(image);
}

uint32_t CreateMaterial(const wchar_t* shaderFile, const D3D12_INPUT_ELEMENT_DESC* layout, UINT layoutCount)
{
    return renderer.UploadMaterial(shaderFile, layout, layoutCount);
}

uint32_t CreateMesh(const void* vertices, UINT vertexBytes, UINT stride, const uint32_t* indices, UINT indexCount)
{
    return renderer.UploadMesh(vertices, vertexBytes, stride, indices, indexCount);
}

uint32_t Spawn(const RenderObject& obj)
{
    renderer.meshes.push_back(obj);
    return static_cast<uint32_t>(renderer.meshes.size() - 1);
}

// Use the reference right away, a later Spawn can move the vector and leave it dangling
RenderObject& GetRenderObject(uint32_t id)
{
    return renderer.meshes[id];
}

float GetDeltaTime() {
    return deltaTime.count();
}

static void UpdateFrameTime() {
    static uint64_t frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    deltaTime = t1 - t0;
    t0 = t1;

    elapsedSeconds += deltaTime.count();

    if (elapsedSeconds > 1.0) {
        char buffer[500];
        auto fps = frameCounter / elapsedSeconds;
        sprintf_s(buffer, 500, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);
        SetWindowTextA(g_hWnd, buffer);

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

// Start and rendering has to be called here in order for SteamWandMain to be able to run.
// These two functions are ugly exceptions since engine to game calls can only go through 
// start and update (declared in app.h), essentially the engine calls code it didn't run so
// it sort of works like a callback to resolve linking errors. Pattern is called "Inversion of control"
// As a result, this is the only source file that you can't run as it's serving like a library that ties things together.
_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    renderer.SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    Start();

    MSG msg = {};

    while (msg.message != WM_QUIT) {
        UpdateFrameTime();
        inputHandler.BeginFrame();

        while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT || inputHandler.isKeyPressed(Button::Escape)) {
            break;
        }

        if (inputHandler.isKeyPressed(Button::F2)) {
            renderer.g_Wireframe = !renderer.g_Wireframe;
        }

        if (inputHandler.isKeyPressed(Button::F3)) {
            renderer.g_ShowDepth = !renderer.g_ShowDepth;
        }

        if (inputHandler.isKeyPressed(Button::V)) {
            renderer.g_VSync = !renderer.g_VSync;
        }

        if (inputHandler.isKeyPressed(Button::F11) ||
            (inputHandler.isKeyDown(Button::Alt) && inputHandler.isKeyPressed(Button::Enter))) {
            SetFullScreen(!g_FullScreen);
        }

        Update();
        renderer.Render();
    }

    renderer.Shutdown();
    return 0;
}
