#include "App.h"

using namespace Microsoft::WRL;

static std::chrono::duration<float> deltaTime{};

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

    Renderer renderer;

    renderer.SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    Start(renderer);

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

        if (inputHandler.isKeyPressed(Button::V)) {
            renderer.g_VSync = !renderer.g_VSync;
        }

        if (inputHandler.isKeyPressed(Button::F11) ||
            (inputHandler.isKeyDown(Button::Alt) && inputHandler.isKeyPressed(Button::Enter))) {
            SetFullScreen(!g_FullScreen);
        }

        Update(renderer);
        renderer.Render();
    }

    renderer.Shutdown();
    return 0;
}
