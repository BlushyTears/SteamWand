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
using namespace Microsoft::WRL;

#include "d3dx12.h"

#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include "Renderer.h"
#include "Window.h"

ComPtr<ID3D12RootSignature> CreateShapeRootSignature(ComPtr<ID3D12Device2> g_Device, HRESULT& hr) {

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

_Use_decl_annotations_
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {

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
    MSG msg = {};

    while (msg.message != WM_QUIT) {
        if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
        else {
            Update();
            Render();
        }
    }

    Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);
    ::CloseHandle(g_FenceEvent);
    return 0;
}
