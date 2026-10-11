#pragma once

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

// The application defines these, the engine's wWinMain (App.cpp) calls them
void Start(Renderer& renderer);
void Update(Renderer& renderer);
float GetDeltaTime();