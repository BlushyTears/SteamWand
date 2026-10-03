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

using Microsoft::WRL::ComPtr;

inline constexpr uint8_t g_NumFrames = 3;

inline bool g_UseWarp = false;

inline uint32_t g_ClientWidth = 1280;
inline uint32_t g_ClientHeight = 720;


inline bool g_IsInitialized = false;

#include "Window.h" // g_IsInitialized needs to be declared before window.h otherwise we get declaration issues, so we should restructure this soon and avoid globals
// maybe make a big struct or something soon

inline ComPtr<ID3D12RootSignature> root_signature;
inline ComPtr<ID3D12Device2> g_Device;
inline ComPtr<ID3D12CommandQueue> g_CommandQueue;
inline ComPtr<IDXGISwapChain4> g_SwapChain;
inline ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
inline ComPtr<ID3D12GraphicsCommandList> g_CommandList;
inline ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
inline ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
inline ComPtr<ID3D12PipelineState> g_PipelineState;
inline ComPtr<ID3D12PipelineState> g_LinePipelineState;
inline ComPtr<ID3D12PipelineState> g_QuadPipelineState;

inline UINT g_RTVDescriptorSize;
inline UINT g_CurrentBackBufferIndex;

inline ComPtr<ID3D12Fence> g_Fence;
inline uint64_t g_FenceValue = 0;
inline uint64_t g_FrameFenceValues[g_NumFrames] = {};
inline HANDLE g_FenceEvent;

inline bool g_VSync = true;
inline bool g_TearingSupported = false;

inline void ThrowIfFailed(HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

inline void EnableDebugLayer() {
#if defined (_DEBUG)
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();
#endif
}

inline ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp) {
    ComPtr<IDXGIFactory4> dxgiFactory;
    UINT createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

    ComPtr<IDXGIAdapter1> dxgiAdapter1;
    ComPtr<IDXGIAdapter4> dxgiAdapter4;

    if (useWarp)
    {
        ThrowIfFailed(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter1)));
        ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
    else {
        SIZE_T maxDedicatedVideoMemory = 0;

        for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
            dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

            if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
                    D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
                dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory) {

                maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
                ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));

            }
        }
    }

    return dxgiAdapter4;
}

inline ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter) {
    ComPtr<ID3D12Device2> d3d12Device2;
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(d3d12Device2.As(&pInfoQueue))) {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};

        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif

    return d3d12Device2;
}

inline ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
    ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));
    return d3d12CommandQueue;
}

inline bool CheckTearingSupport() {
    BOOL allowTearing = false;

    ComPtr<IDXGIFactory4> factory4;

    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)))) {
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(factory4.As(&factory5))) {
            if (FAILED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allowTearing, sizeof(allowTearing)
            )))
            {
                allowTearing = false;
            }
        }
    }

    return allowTearing == TRUE;
}

inline ComPtr<IDXGISwapChain4> CreateSwapChain(
    HWND hWnd,
    ComPtr<ID3D12CommandQueue> commandQueue,
    uint32_t width,
    uint32_t height,
    uint32_t bufferCount)
{
    ComPtr<IDXGISwapChain4> dxgiSwapChain4;
    ComPtr<IDXGIFactory4> dxgiFactory4;

    UINT createFactoryFlags = 0;

#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    ThrowIfFailed(
        CreateDXGIFactory2(
            createFactoryFlags,
            IID_PPV_ARGS(&dxgiFactory4)
        )
    );

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;

    swapChainDesc.SampleDesc = { 1, 0 };

    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = bufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

    swapChainDesc.Flags =
        CheckTearingSupport()
        ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
        : 0;

    ComPtr<IDXGISwapChain1> swapChain1;

    ThrowIfFailed(
        dxgiFactory4->CreateSwapChainForHwnd(
            commandQueue.Get(),
            hWnd,
            &swapChainDesc,
            nullptr,
            nullptr,
            &swapChain1
        )
    );

    ThrowIfFailed(
        dxgiFactory4->MakeWindowAssociation(
            hWnd,
            DXGI_MWA_NO_ALT_ENTER
        )
    );

    ThrowIfFailed(
        swapChain1.As(&dxgiSwapChain4)
    );

    return dxgiSwapChain4;
}

inline ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device2> device,
    D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors) {

    ComPtr<ID3D12DescriptorHeap> descriptorHeap;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = numDescriptors;
    desc.Type = type;

    ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

inline void UpdateRenderTargetViews(ComPtr<ID3D12Device2> device,
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

inline ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type) {
    ComPtr<ID3D12CommandAllocator> commandAllocator;

    ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

inline ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device, ComPtr<ID3D12CommandAllocator> commandAllocator,
    D3D12_COMMAND_LIST_TYPE type) {

    ComPtr<ID3D12GraphicsCommandList> commandList;

    ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

    ThrowIfFailed(commandList->Close());

    return commandList;
}

inline ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device) {

    ComPtr<ID3D12Fence> fence;

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

    return fence;
}

inline HANDLE CreateEventHandle() {
    HANDLE fenceEvent;

    fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

    assert(fenceEvent && "Failed to create fence event");

    return fenceEvent;
}

inline uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue) {
    uint64_t fenceValueForSignal = ++fenceValue;

    ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

    return fenceValueForSignal;
}

inline void WaitForFenceValue(
    ComPtr<ID3D12Fence> fence,
    uint64_t fenceValue,
    HANDLE fenceEvent)
{
    if (fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(
            fence->SetEventOnCompletion(
                fenceValue,
                fenceEvent
            )
        );

        ::WaitForSingleObject(
            fenceEvent,
            INFINITE
        );
    }
}

inline void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent) {

    uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);

    WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

inline void Update() {
    static uint64_t frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    t0 = t1;

    elapsedSeconds += deltaTime.count() * 1e-9;

    if (elapsedSeconds > 1.0) {
        char buffer[500];
        auto fps = frameCounter / elapsedSeconds;
        sprintf_s(buffer, 500, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

inline void Render() {
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

        g_CommandList->SetPipelineState(g_LinePipelineState.Get());
        g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

        for (const Shapes::VerticalLine& line : Shapes::g_Lines) {
            float values[] = {
                line.x,
                line.y,
                line.size
            };

            g_CommandList->SetGraphicsRoot32BitConstants(0, 3, values, 0);
            g_CommandList->DrawInstanced(2, 1, 0, 0);
        }

        g_CommandList->SetPipelineState(g_PipelineState.Get());
        g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (const Shapes::Triangle& triangle : Shapes::g_Triangles) {
            float values[] = {
                triangle.x,
                triangle.y,
                triangle.size
            };

            g_CommandList->SetGraphicsRoot32BitConstants(0, 3, values, 0);
            g_CommandList->DrawInstanced(3, 1, 0, 0);
        }

        g_CommandList->SetPipelineState(g_QuadPipelineState.Get());
        g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (const Shapes::Quad& quad : Shapes::g_Quads) {
            float values[] = {
                quad.x,
                quad.y,
                quad.size,
            };

            g_CommandList->SetGraphicsRoot32BitConstants(0, 3, values, 0);
            g_CommandList->DrawInstanced(6, 1, 0, 0);
        }
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

inline void Resize(uint32_t width, uint32_t height) {
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

inline void CreatePipelinePrimitive(D3D12_GRAPHICS_PIPELINE_STATE_DESC& pso,
    std::string vertexShaderProgram, std::string pixelShaderProgram,
    ComPtr<ID3D12PipelineState>& g_GenericPipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType) {

    ComPtr<ID3DBlob> shaderErrors;

    auto vertexShader = CompileShader(L"Shapes.hlsl", vertexShaderProgram.c_str(), "vs_5_0");
    auto pixelShader = CompileShader(L"Shapes.hlsl", pixelShaderProgram.c_str(), "ps_5_0");

    // This pso is hard coded, at some point we probably want this to lie in some better place
    pso.pRootSignature = root_signature.Get();
    pso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    pso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

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

    D3D12_ROOT_PARAMETER root_parameters[1] = {};
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_parameters[0].Constants.Num32BitValues = 3;
    root_parameters[0].Constants.ShaderRegister = 0;
    root_parameters[0].Constants.RegisterSpace = 0;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.NumParameters = _countof(root_parameters);
    root_signature_desc.pParameters = root_parameters;
    root_signature_desc.NumStaticSamplers = 0;
    root_signature_desc.pStaticSamplers = nullptr;
    root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature_blob;
    ComPtr<ID3DBlob> error_blob;

    hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob);
    ThrowIfFailed(hr);

    hr = g_Device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&signature));

    ThrowIfFailed(hr);

    return signature;
}


inline void SetupVariables(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const wchar_t* windowClassName = L"DX12WindowClass";

    EnableDebugLayer();

    g_TearingSupported = CheckTearingSupport();

    RegisterWindowClass(hInstance, windowClassName, WndProc);

    g_hWnd = CreateAppWindow(windowClassName, hInstance, L"Learning DirectX12", g_ClientWidth, g_ClientHeight);

    ::GetWindowRect(g_hWnd, &g_WindowRect);

    ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(g_UseWarp);

    g_Device = CreateDevice(dxgiAdapter4);
    g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    g_SwapChain = CreateSwapChain(g_hWnd, g_CommandQueue, g_ClientWidth, g_ClientHeight, g_NumFrames);
    g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

    g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
    g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePso = {};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPso = {};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC linePso = {};
    HRESULT hr;

    root_signature = CreateShapeRootSignature(g_Device, hr);

    CreatePipelinePrimitive(trianglePso, "VSMain", "PSMain", g_PipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    CreatePipelinePrimitive(quadPso, "VSQuad", "PSQuad", g_QuadPipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    CreatePipelinePrimitive(linePso, "VSLine", "PSLine", g_LinePipelineState, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);

    for (int i = 0; i < g_NumFrames; i++) {
        g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    g_CommandList = CreateCommandList(g_Device, g_CommandAllocators[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

    g_Fence = CreateFence(g_Device);
    g_FenceEvent = CreateEventHandle();

    g_IsInitialized = true;
    ::ShowWindow(g_hWnd, SW_SHOW);


}