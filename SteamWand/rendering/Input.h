#pragma once

#include <Windows.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cwchar>
#include <vector>
#include "Shapes.h"
#include <array>

#include <windowsx.h>

enum class Button {
    // Mouse buttons
    MouseLeft = VK_LBUTTON,
    MouseRight = VK_RBUTTON,
    MouseMiddle = VK_MBUTTON,
    MouseSide1 = VK_XBUTTON1,
    MouseSide2 = VK_XBUTTON2,

    // Letters
    A = 'A', B = 'B', C = 'C', D = 'D',
    E = 'E', F = 'F', G = 'G', H = 'H',
    I = 'I', J = 'J', K = 'K', L = 'L',
    M = 'M', N = 'N', O = 'O', P = 'P',
    Q = 'Q', R = 'R', S = 'S', T = 'T',
    U = 'U', V = 'V', W = 'W', X = 'X',
    Y = 'Y', Z = 'Z',

    // Number row
    Digit0 = '0', Digit1 = '1',
    Digit2 = '2', Digit3 = '3',
    Digit4 = '4', Digit5 = '5',
    Digit6 = '6', Digit7 = '7',
    Digit8 = '8', Digit9 = '9',

    // Common keys
    Space = VK_SPACE,
    Enter = VK_RETURN,
    Escape = VK_ESCAPE,
    Tab = VK_TAB,
    Backspace = VK_BACK,

    // Modifiers
    Shift = VK_SHIFT,
    Control = VK_CONTROL,
    Alt = VK_MENU,

    LeftShift = VK_LSHIFT,
    RightShift = VK_RSHIFT,
    LeftControl = VK_LCONTROL,
    RightControl = VK_RCONTROL,
    LeftAlt = VK_LMENU,
    RightAlt = VK_RMENU,

    // Arrow keys
    Left = VK_LEFT,
    Right = VK_RIGHT,
    Up = VK_UP,
    Down = VK_DOWN,

    // Navigation and editing
    Insert = VK_INSERT,
    Delete = VK_DELETE,
    Home = VK_HOME,
    End = VK_END,
    PageUp = VK_PRIOR,
    PageDown = VK_NEXT,

    // Locks and system keys
    CapsLock = VK_CAPITAL,
    NumLock = VK_NUMLOCK,
    ScrollLock = VK_SCROLL,
    PrintScreen = VK_SNAPSHOT,
    Pause = VK_PAUSE,
    LeftWindows = VK_LWIN,
    RightWindows = VK_RWIN,
    ContextMenu = VK_APPS,

    // Function keys
    F1 = VK_F1, F2 = VK_F2, F3 = VK_F3,
    F4 = VK_F4, F5 = VK_F5, F6 = VK_F6,
    F7 = VK_F7, F8 = VK_F8, F9 = VK_F9,
    F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,

    // Number pad
    Numpad0 = VK_NUMPAD0,
    Numpad1 = VK_NUMPAD1,
    Numpad2 = VK_NUMPAD2,
    Numpad3 = VK_NUMPAD3,
    Numpad4 = VK_NUMPAD4,
    Numpad5 = VK_NUMPAD5,
    Numpad6 = VK_NUMPAD6,
    Numpad7 = VK_NUMPAD7,
    Numpad8 = VK_NUMPAD8,
    Numpad9 = VK_NUMPAD9,

    NumpadAdd = VK_ADD,
    NumpadSubtract = VK_SUBTRACT,
    NumpadMultiply = VK_MULTIPLY,
    NumpadDivide = VK_DIVIDE,
    NumpadDecimal = VK_DECIMAL,

    // Punctuation: US labels; symbols vary by keyboard layout
    Semicolon = VK_OEM_1,
    Equals = VK_OEM_PLUS,
    Comma = VK_OEM_COMMA,
    Minus = VK_OEM_MINUS,
    Period = VK_OEM_PERIOD,
    Slash = VK_OEM_2,
    Backtick = VK_OEM_3,
    LeftBracket = VK_OEM_4,
    Backslash = VK_OEM_5,
    RightBracket = VK_OEM_6,
    Apostrophe = VK_OEM_7,
    ExtraLayoutKey = VK_OEM_102
};

struct ButtonState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct InputHandler {
    std::array<ButtonState, 256> all_buttons{};

    void BeginFrame() {
        for (auto& button : all_buttons) {
            button.pressed = false;
            button.released = false;
        }

        scrollDelta = 0;
    }
        
void CheckInputs(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN ||
        message == WM_RBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_RBUTTONUP) {

        mouseX = GET_X_LPARAM(lParam);
        mouseY = GET_Y_LPARAM(lParam);

    }

    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            auto& button = all_buttons[wParam];

            if (!button.down) {
                button.pressed = true;
            }
            button.down = true;
            break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            auto& button = all_buttons[wParam];

            if (button.down) {
                button.released = true;
            }
            button.down = false;
            break;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        {
            int key = (message == WM_LBUTTONDOWN) ? VK_LBUTTON : VK_RBUTTON;

            auto& button = all_buttons[key];

            if (!button.down)
                button.pressed = true;
            button.down = true;
            break;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        {
            int key = (message == WM_LBUTTONUP) ? VK_LBUTTON : VK_RBUTTON;

            auto& button = all_buttons[key];

            if (button.down)
                button.released = true;
            button.down = false;
            break;
        }
        case WM_MOUSEWHEEL:
        {
            scrollDelta += GET_WHEEL_DELTA_WPARAM(wParam);
            break;
        }
        }
    }

    // Held
    bool isKeyDown(Button button) const {
        return all_buttons[static_cast<int>(button)].down;
    }
    
    // Released
    bool isKeyReleased(Button button) const {
        return all_buttons[static_cast<int>(button)].released;
    }

    // 1-click
    bool isKeyPressed(Button button) const {
        return all_buttons[static_cast<int>(button)].pressed;
    }

    bool isMouseScrollingForward() {
        return scrollDelta > 0;
    }

    bool isMouseScrollingBackward() {
        return scrollDelta < 0;
    }

    bool isMouseScrolling() {
        return scrollDelta != 0;
    }

    int scrollDelta = 0;
    int mouseX = 0;
    int mouseY = 0;
};

// This should probably be stored somewhere else, but since we're early it seems pointless to think ahead for refactors like this
inline InputHandler inputHandler;
