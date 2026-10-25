#pragma once

#include <vector>
#include <chrono>
#include <algorithm>
#include <cstdio>

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <cstdint>
#include <exception>

#include "d3dx12.h"
#include "Shapes.h"

#include "../math/Vec3.h"
#include "../math/Vec4.h"
#include "../math/Mat4.h"
#include "../math/Math.h"

#include <cmath>

#include "Camera.h"
#include <cstring>
#include "Texture.h"
#include "D3DUtil.h"
#include <span>

inline constexpr uint8_t g_NumFrames = 3;

struct GPUMesh {
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;

    D3D12_VERTEX_BUFFER_VIEW vertexView = {};
    D3D12_INDEX_BUFFER_VIEW indexView = {};

    UINT indexCount = 0;
};

struct Material {
    ComPtr<ID3D12PipelineState> solid;
    ComPtr<ID3D12PipelineState> wireframe;
};

inline const D3D12_INPUT_ELEMENT_DESC g_TexturedLayout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

inline const D3D12_INPUT_ELEMENT_DESC g_ColoredLayout[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

template<typename V> struct VertexLayout;
template<> struct VertexLayout<Shapes::Vertex> {
    static std::span<const D3D12_INPUT_ELEMENT_DESC> get() { return g_TexturedLayout; }
};

template<> struct VertexLayout<ColoredShapes::Vertex> {
    static std::span<const D3D12_INPUT_ELEMENT_DESC> get() { return g_ColoredLayout; }
};

struct RenderObject {
    GPUMesh mesh;
    vec3 position;
    vec3 scale = {1, 1, 1};
    vec3 rotation = {0, 0, 0};
    uint32_t material;
    uint32_t texture = 0;
};

struct GPUTexture {
    ComPtr<ID3D12Resource> resource;
    uint32_t srvIndex = 0;
};

struct Renderer {
    ComPtr<ID3D12DescriptorHeap> g_SRVDescriptorHeap;
    ComPtr<ID3D12GraphicsCommandList> g_CommandList;
    ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
    ComPtr<ID3D12CommandQueue> g_CommandQueue;
    ComPtr<IDXGISwapChain4> g_SwapChain;
    ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;

    std::vector<Material> materials;
    ComPtr<ID3D12RootSignature> root_signature;
    ComPtr<ID3D12Device2> g_Device;
    ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];

    ComPtr<ID3D12Fence> g_Fence;
    uint64_t g_FenceValue = 0;
    uint64_t g_FrameFenceValues[g_NumFrames] = {};
    HANDLE g_FenceEvent;

    std::vector<RenderObject> meshes;
    std::vector<GPUTexture> textures;
    static constexpr int kMaxTextures = 256;
    uint32_t nextSRVIndex = 0;

    bool g_VSync = true;
    bool g_TearingSupported = false;
    bool g_UseWarp = false;
    bool g_Wireframe = false;

    bool g_IsInitialized = false;

    void Render();
    void SetupVariables(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow);
    void CreatePipelinePrimitive(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso,
        const wchar_t* shaderFile, std::string vertexShaderProgram, std::string pixelShaderProgram,
        ComPtr<ID3D12PipelineState>& g_GenericPipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType, D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID);

    void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device,
        ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);

    void Resize(uint32_t width, uint32_t height);

    ComPtr<ID3D12Resource> CreateUploadBuffer(const void* data, size_t byteCount);
    template<typename Vertex>

    GPUMesh UploadMesh(const Mesh<Vertex>& mesh);
    uint32_t UploadTexture(const Image& image);
    void DrawMesh(const RenderObject& obj);
    void DrawMeshes();

    uint32_t CreateMaterial(const wchar_t* shaderFile, const D3D12_INPUT_ELEMENT_DESC* layout, UINT layoutCount);

    template<typename V>
    uint32_t CreateMaterial(const wchar_t* shaderFile) {
        auto layout = VertexLayout<V>::get();
        return CreateMaterial(shaderFile, layout.data(), static_cast<UINT>(layout.size()));
    }

    void Shutdown();

    UINT g_RTVDescriptorSize = 0;
    UINT g_CurrentBackBufferIndex = 0;
    UINT g_SRVDescriptorSize = 0;
};

// This has to be put akwardly here annoyingly so
#include "Window.h"

inline uint32_t g_ClientWidth = 1280;
inline uint32_t g_ClientHeight = 720;

inline void Renderer::UpdateRenderTargetViews(ComPtr<ID3D12Device2> device,
    ComPtr<IDXGISwapChain4> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap) {

    auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < g_NumFrames; ++i) {
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

        device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

        g_BackBuffers[i] = backBuffer;
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

inline void Renderer::Shutdown() {
    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
    ::CloseHandle(g_FenceEvent);
    g_FenceEvent = nullptr;
}

inline void Renderer::Render() {
    auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    auto backBuffer = g_BackBuffers[g_CurrentBackBufferIndex];

    commandAllocator->Reset();
    g_CommandList->Reset(commandAllocator.Get(), nullptr);

    {
        // Present frame
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(),
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        g_CommandList->ResourceBarrier(1, &barrier);

        FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(g_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_CurrentBackBufferIndex, g_RTVDescriptorSize);
        g_CommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        D3D12_VIEWPORT viewPort = {
            0.0f, 0.0f,
            static_cast<float>(g_ClientWidth),
            static_cast<float>(g_ClientHeight),
            0.0f, 1.0f
        };

        D3D12_RECT scissor = {
            0, 0,
            static_cast<LONG>(g_ClientWidth),
            static_cast<LONG>(g_ClientHeight)
        };

        g_CommandList->RSSetViewports(1, &viewPort);
        g_CommandList->RSSetScissorRects(1, &scissor);
        g_CommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        g_CommandList->SetGraphicsRootSignature(root_signature.Get());

        ID3D12DescriptorHeap* heaps[] = {
            g_SRVDescriptorHeap.Get()
        };

        g_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);
        //g_CommandList->SetGraphicsRootDescriptorTable(1, g_SRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

        float aspect = float(g_ClientWidth) / float(g_ClientHeight);
        mat4 look = lookat(g_CameraPosition, g_CameraPosition + GetCameraForward(), vec3(0.0f, 1.0f, 0.0f));
        mat4 viewProjection = perspective(radians(67.0f), aspect, 0.1f, 100.0f) * look;

        g_CommandList->SetGraphicsRoot32BitConstants(0, 16, &viewProjection.vec[0].x, 16);
        DrawMeshes();
    }

    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

        g_CommandList->ResourceBarrier(1, &barrier);

        ThrowIfFailed(g_CommandList->Close());

        ID3D12CommandList* const commandList[] = { g_CommandList.Get() };

        g_CommandQueue->ExecuteCommandLists(_countof(commandList), commandList);

        g_FrameFenceValues[g_CurrentBackBufferIndex] = Signal(g_CommandQueue, g_Fence, g_FenceValue);

        UINT syncInterval = g_VSync ? 1 : 0;
        UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
        ThrowIfFailed(g_SwapChain->Present(syncInterval, presentFlags));

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        WaitForFenceValue(g_Fence, g_FrameFenceValues[g_CurrentBackBufferIndex], g_FenceEvent);
    }
}

inline void Renderer::Resize(uint32_t width, uint32_t height) {
    if (g_ClientWidth != width || g_ClientHeight != height) {
        g_ClientWidth = std::max(1u, width);
        g_ClientHeight = std::max(1u, height);

        Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

        for (int i = 0; i < g_NumFrames; ++i) {
            g_BackBuffers[i].Reset();
            g_FrameFenceValues[i] = g_FrameFenceValues[g_CurrentBackBufferIndex];
        }

        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 1, 0 };
        ThrowIfFailed(g_SwapChain->GetDesc(&swapChainDesc));
        ThrowIfFailed(g_SwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

        g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);
    }
}

inline ComPtr<ID3D12Resource> Renderer::CreateUploadBuffer(const void* data, size_t byteCount) {
    ComPtr<ID3D12Resource> buffer;

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);

    auto description = CD3DX12_RESOURCE_DESC::Buffer(byteCount);

    ThrowIfFailed(g_Device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));

    void* destination = nullptr;
    D3D12_RANGE readRange = { 0, 0 };

    ThrowIfFailed(buffer->Map(0, &readRange, &destination));

    std::memcpy(destination, data, byteCount);

    buffer->Unmap(0, nullptr);

    return buffer;
}

template<typename Vertex>
GPUMesh Renderer::UploadMesh(const Mesh<Vertex>& mesh) {
    GPUMesh gpu;

    UINT vertexBytes = static_cast<UINT>(mesh.vertices.size() * sizeof(Vertex));

    UINT indexBytes = static_cast<UINT>(mesh.indices.size() * sizeof(uint32_t));

    gpu.vertexBuffer = CreateUploadBuffer(mesh.vertices.data(), vertexBytes);
    gpu.indexBuffer = CreateUploadBuffer(mesh.indices.data(), indexBytes);

    gpu.vertexView.BufferLocation = gpu.vertexBuffer->GetGPUVirtualAddress();
    gpu.vertexView.SizeInBytes = vertexBytes;
    gpu.vertexView.StrideInBytes = sizeof(Vertex);

    gpu.indexView.BufferLocation = gpu.indexBuffer->GetGPUVirtualAddress();
    gpu.indexView.SizeInBytes = indexBytes;
    gpu.indexView.Format = DXGI_FORMAT_R32_UINT;

    gpu.indexCount = static_cast<UINT>(mesh.indices.size());

    return gpu;
}

inline uint32_t Renderer::UploadTexture(const Image& image) {
    GPUTexture gpu;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    auto textureDescription = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, image.width, image.height, 1, 1);

    ThrowIfFailed(g_Device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &textureDescription, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpu.resource)));

    ComPtr<ID3D12Resource> uploadBuffer;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDescription = CD3DX12_RESOURCE_DESC::Buffer(GetRequiredIntermediateSize(gpu.resource.Get(), 0, 1));

    ThrowIfFailed(g_Device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDescription, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer)));

    auto commandAllocator = g_CommandAllocators[g_CurrentBackBufferIndex];
    commandAllocator->Reset();
    g_CommandList->Reset(commandAllocator.Get(), nullptr);

    // Pads each row to 256 bytes in the upload buffer and records the copy into the txt
    D3D12_SUBRESOURCE_DATA pixels = { image.pixels.data(), image.width * 4, image.width * 4 * image.height };
    UpdateSubresources(g_CommandList.Get(), gpu.resource.Get(), uploadBuffer.Get(), 0, 0, 1, &pixels);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(gpu.resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    g_CommandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(g_CommandList->Close());

    ID3D12CommandList* const commandList[] = { g_CommandList.Get() };
    g_CommandQueue->ExecuteCommandLists(_countof(commandList), commandList);

    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

    assert(nextSRVIndex < kMaxTextures);

    gpu.srvIndex = nextSRVIndex++;

    CD3DX12_CPU_DESCRIPTOR_HANDLE slot(
        g_SRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
        gpu.srvIndex, g_SRVDescriptorSize);

    textures.push_back(gpu);
    g_Device->CreateShaderResourceView(gpu.resource.Get(), nullptr, slot);

    return static_cast<uint32_t>(textures.size() - 1);
}

inline uint32_t Renderer::CreateMaterial(const wchar_t* shaderFile, const D3D12_INPUT_ELEMENT_DESC* layout, UINT layoutCount) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};

    pso.InputLayout = { layout, layoutCount};

    Material material;

    CreatePipelinePrimitive(pso, shaderFile, "VSMesh", "PSMesh", material.solid, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    CreatePipelinePrimitive(pso, shaderFile, "VSMesh", "PSMesh", material.wireframe, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, D3D12_FILL_MODE_WIREFRAME);

    materials.push_back(material);

    return static_cast<uint32_t>(materials.size() - 1);
}

inline void Renderer::DrawMeshes() {
    for (const RenderObject& obj : meshes) {
        DrawMesh(obj);
    }
}

inline void Renderer::DrawMesh(const RenderObject& obj) {
    const Material& mat = materials[obj.material];
    g_CommandList->SetPipelineState(g_Wireframe ? mat.wireframe.Get() : mat.solid.Get());

    g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    g_CommandList->IASetVertexBuffers(0, 1, &obj.mesh.vertexView);

    g_CommandList->IASetIndexBuffer(&obj.mesh.indexView);

    mat4 scale;
    scale.vec[0].x = obj.scale.x;
    scale.vec[1].y = obj.scale.y;
    scale.vec[2].z = obj.scale.z;

    mat4 rotation = rotationx(obj.rotation.x) * rotationy(obj.rotation.y) * rotationz(obj.rotation.z);

    mat4 translation;
    translation.vec[3] = vec4(obj.position.x, obj.position.y, obj.position.z, 1.0f);

    mat4 model = translation * rotation * scale;

    const GPUTexture& tex = textures[obj.texture];

    CD3DX12_GPU_DESCRIPTOR_HANDLE srv(
        g_SRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
        tex.srvIndex, g_SRVDescriptorSize);
    g_CommandList->SetGraphicsRootDescriptorTable(1, srv);

    g_CommandList->SetGraphicsRoot32BitConstants(0, 16, &model.vec[0].x, 0);
    g_CommandList->DrawIndexedInstanced(obj.mesh.indexCount, 1, 0, 0, 0);
}

inline ComPtr<ID3DBlob> CompileShader(const wchar_t* file, const char* entryPoint, const char* target) {
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;

    HRESULT hr = D3DCompileFromFile(
        file, nullptr, nullptr,
        entryPoint, target,
        0, 0,
        &shader, &errors);

    if (errors) {
        OutputDebugStringA(
            static_cast<const char*>(errors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);
    return shader;
}

inline void Renderer::CreatePipelinePrimitive(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso, const wchar_t* shaderFile,
    std::string vertexShaderProgram, std::string pixelShaderProgram,
    ComPtr<ID3D12PipelineState>& g_GenericPipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType,
    D3D12_FILL_MODE fillMode) {

    ComPtr<ID3DBlob> shaderErrors;

    // this is probably not gonna work once we reference the shader from elsewhere, so we might need a relative path that's more scalable
    // both for other projects liek SteamWandMain to reference, but also future builds
    auto vertexShader = CompileShader(shaderFile, vertexShaderProgram.c_str(), "vs_5_0");
    auto pixelShader = CompileShader(shaderFile, pixelShaderProgram.c_str(), "ps_5_0");

    pso.pRootSignature = root_signature.Get();
    pso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.FillMode = fillMode;

    if (fillMode == D3D12_FILL_MODE_WIREFRAME) {
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    }
    else {
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    }

    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = topologyType;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_GenericPipelineState)));

    shaderErrors.Reset();
}

inline ComPtr<ID3D12RootSignature> CreateShapeRootSignature(ComPtr<ID3D12Device2> g_Device, HRESULT& hr) {
    ComPtr<ID3D12RootSignature> signature;

    D3D12_DESCRIPTOR_RANGE root_range = {};
    root_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    root_range.NumDescriptors = 1;
    root_range.BaseShaderRegister = 0;
    root_range.RegisterSpace = 0;
    root_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.ShaderRegister = 0;
    samplerDesc.RegisterSpace = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_PARAMETER root_parameters[2] = {};
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_parameters[0].Constants.Num32BitValues = 32;
    root_parameters[0].Constants.ShaderRegister = 0;
    root_parameters[0].Constants.RegisterSpace = 0;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    root_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[1].DescriptorTable.pDescriptorRanges = &root_range;
    root_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters = _countof(root_parameters);
    root_signature_desc.pParameters = root_parameters;
    root_signature_desc.NumStaticSamplers = 1;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.pStaticSamplers = &samplerDesc;

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;

    hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob);

    if (error_blob) {
        OutputDebugStringA(static_cast<const char*>(error_blob->GetBufferPointer()));
    }
    ThrowIfFailed(hr);

    hr = g_Device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&signature));
    ThrowIfFailed(hr);

    return signature;
}

inline void Renderer::SetupVariables(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t* windowClassName = L"DX12WindowClass";

    EnableDebugLayer();

    g_TearingSupported = CheckTearingSupport();

    RegisterWindowClass(hInstance, windowClassName, WndProc);

    g_hWnd = CreateAppWindow(windowClassName, hInstance, L"Learning DirectX12", g_ClientWidth, g_ClientHeight);

    ::SetWindowLongPtrW(g_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    ::GetWindowRect(g_hWnd, &g_WindowRect);

    ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(g_UseWarp);

    g_Device = CreateDevice(dxgiAdapter4);
    g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    g_SwapChain = CreateSwapChain(g_hWnd, g_CommandQueue, g_ClientWidth, g_ClientHeight, g_NumFrames);
    g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

    g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
    g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    g_SRVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxTextures, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);


    g_SRVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);

    HRESULT hr;

    root_signature = CreateShapeRootSignature(g_Device, hr);

    for (int i = 0; i < g_NumFrames; i++) {
        g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    g_CommandList = CreateCommandList(g_Device, g_CommandAllocators[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

    g_Fence = CreateFence(g_Device);
    g_FenceEvent = CreateEventHandle();

    g_IsInitialized = true;
    ::ShowWindow(g_hWnd, SW_SHOW);
}