#pragma once

#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cwchar>

#include "Input.h"

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
    inputHandler.CheckInputs(hwnd, message, wParam, lParam);

    switch (message)
    {
    case WM_SIZE:
        if (g_IsInitialized)
        {
            RECT clientRect = {};
            ::GetClientRect(hwnd, &clientRect);
            Resize(clientRect.right - clientRect.left,
                clientRect.bottom - clientRect.top);
        }
        return 0;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    case WM_SYSKEYDOWN:
    case WM_SYSCHAR:
        // Alt+Enter is handled in the frame loop, not by the window menu.
        if (wParam == VK_RETURN)
            return 0;
        break;
    }

    // Windows handles painting and unhandled system keys, including Alt+F4.
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
}
