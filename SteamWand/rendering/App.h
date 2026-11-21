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

float GetDeltaTime();

// The application defines these, the engine's wWinMain (App.cpp) calls them
void Start();
void Update();

uint32_t CreateTexture(const Image& image);
uint32_t CreateMaterial(const wchar_t* shaderFile, const D3D12_INPUT_ELEMENT_DESC* layout, UINT layoutCount);
uint32_t CreateMesh(const void* vertices, UINT vertexBytes, UINT stride, const uint32_t* indices, UINT indexCount);
uint32_t Spawn(const RenderObject& obj);
RenderObject& GetRenderObject(uint32_t id);

// Templates must be in the header, so these only turn the type into raw data for the functions above
template<typename V>
uint32_t CreateMaterial(const wchar_t* shaderFile) {
    auto layout = VertexLayout<V>::get();
    return CreateMaterial(shaderFile, layout.data(), static_cast<UINT>(layout.size()));
}

template<typename V>
uint32_t CreateMesh(const Mesh<V>& mesh) {
    return CreateMesh(mesh.vertices.data(), static_cast<UINT>(mesh.vertices.size() * sizeof(V)), sizeof(V),
        mesh.indices.data(), static_cast<UINT>(mesh.indices.size()));
}
