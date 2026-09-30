#pragma once

#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cwchar>

#include "Shapes.h"

inline HWND g_hWnd = nullptr;
inline RECT g_WindowRect = {};
inline bool g_FullScreen = false;

inline HWND CreateAppWindow(const wchar_t* windowClassName, HINSTANCE hInst,
    const wchar_t* windowTitle, uint32_t width, uint32_t height) {

    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    HWND hWnd = CreateWindowExW(
        NULL,
        windowClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        hInst,
        nullptr
    );

    assert(hWnd && "Failed To create window");
    return hWnd;
}

inline void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName, WNDPROC windowProc)
{
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = windowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInst;
    windowClass.hIcon = nullptr;
    windowClass.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = nullptr;

    ATOM atom = ::RegisterClassExW(&windowClass);

    if (atom == 0)
    {
        DWORD error = GetLastError();

        wchar_t buffer[256];
        swprintf_s(
            buffer,
            L"RegisterClassExW failed! Error: %lu\n",
            error
        );

        OutputDebugStringW(buffer);

        assert(false && "Failed to register window class");
    }
}

inline void SetFullScreen(bool fullScreen) {
    if (g_FullScreen == fullScreen)
        return;

    g_FullScreen = fullScreen;

    if (g_FullScreen) {
        ::GetWindowRect(g_hWnd, &g_WindowRect);

        UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

        ::SetWindowLongW(g_hWnd, GWL_STYLE, windowStyle);

        HMONITOR hMonitor = ::MonitorFromWindow(g_hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFOEX);
        ::GetMonitorInfo(hMonitor, &monitorInfo);
        ::SetWindowPos(g_hWnd, HWND_TOP,
            monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(g_hWnd, SW_SHOW);

    }
    else {
        ::SetWindowLong(g_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        ::SetWindowPos(g_hWnd, HWND_NOTOPMOST,
            g_WindowRect.left,
            g_WindowRect.top,
            g_WindowRect.right - g_WindowRect.left,
            g_WindowRect.bottom - g_WindowRect.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ::ShowWindow(g_hWnd, SW_NORMAL);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_IsInitialized)
    {
        switch (message)
        {
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            ::BeginPaint(hwnd, &ps);
            ::EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            float x = 2.0f * mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * mouseY / g_ClientHeight;

            Shapes::AddTriangle(x, y, 0.2f);
            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            float x = 2.0f * mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * mouseY / g_ClientHeight;

            Shapes::AddVerticalLine(x - 0.001f, y, 0.2f);
            Shapes::AddVerticalLine(x, y, 0.2f);
            Shapes::AddVerticalLine(x + 0.001f, y, 0.2f);
            return 0;
        }
        case WM_MOUSEWHEEL:
        {
            POINT mousePos = {
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam)
            };

            if (!ScreenToClient(hwnd, &mousePos))
                return 0;

            float x = 2.0f * mousePos.x / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * mousePos.y / g_ClientHeight;

            Shapes::AddQuad(x, y, 0.2f);
            return 0;
        }
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
            bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            switch (wParam)
            {
            case 'V':
                g_VSync = !g_VSync;
                break;
            case VK_ESCAPE:
                ::PostQuitMessage(0);
                break;
            case VK_RETURN:
                if (alt)
                {
            case VK_F11:
                SetFullScreen(!g_FullScreen);
                }
                break;
            }
        }
        break;
        case WM_SYSCHAR:
            break;
        case WM_SIZE:
        {
            RECT clientRect = {};
            ::GetClientRect(g_hWnd, &clientRect);

            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;

            Resize(width, height);
        }
        break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            break;
        default:
            return ::DefWindowProcW(hwnd, message, wParam, lParam);
        }
    }
    else
    {
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return 0;
}