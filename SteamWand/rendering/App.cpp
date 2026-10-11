#include "App.h"

using namespace Microsoft::WRL;

void MoveAround(Renderer& renderer) {
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

// Start and rendering has to be called here in order for SteamWandMain to be able to run.
// These two functions are ugly exceptions since engine to game calls can only go through 
// start and update (declared in app.h), essentially the engine calls code it didn't run so
// it sort of works like a callback to resolve linking errors. Pattern is called "Inversion of control"
_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    Renderer renderer;

    renderer.SetupVariables(hInstance, hPrevInstance, lpCmdLine, nCmdShow);

    Start(renderer);

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

        MoveAround(renderer);
        Update(renderer, renderer.deltaTime.count());
        renderer.Update();
        renderer.Render();
    }

    renderer.Shutdown();
    return 0;
}
