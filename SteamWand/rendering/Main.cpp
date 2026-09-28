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

void AddVerticalLine(float x, float y, float size) {
    g_Lines.push_back({ x, y, size });
}

void AddQuad(float x, float y, float size) {
    g_Quads.push_back({ x, y, size });
}

void AddTriangle(float x, float y, float size) {
    g_Triangles.push_back({ x, y, size });
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

            AddTriangle(x, y, 0.2f);
            return 0;
        }
        case WM_RBUTTONDOWN:
        {
            float mouseX = static_cast<float>(GET_X_LPARAM(lParam));
            float mouseY = static_cast<float>(GET_Y_LPARAM(lParam));

            float x = 2.0f * mouseX / g_ClientWidth - 1.0f;
            float y = 1.0f - 2.0f * mouseY / g_ClientHeight;

            AddVerticalLine(x - 0.001f, y, 0.2f);
            AddVerticalLine(x, y, 0.2f);
            AddVerticalLine(x + 0.001f, y, 0.2f);
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

            AddQuad(x, y, 0.2f);
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

// Goal: Draw quads by combining two triangles

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

    HRESULT hr = D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature_blob, &error_blob);
    ThrowIfFailed(hr);

    hr = g_Device->CreateRootSignature(0, signature_blob->GetBufferPointer(), signature_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));

    ThrowIfFailed(hr);

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> shaderErrors;

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vertexShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    shaderErrors.Reset();

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &pixelShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePso = {};
    trianglePso.pRootSignature = root_signature.Get();
    trianglePso.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    trianglePso.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

    trianglePso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    trianglePso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    trianglePso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    trianglePso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    trianglePso.DepthStencilState.DepthEnable = FALSE;
    trianglePso.DepthStencilState.StencilEnable = FALSE;

    trianglePso.SampleMask = UINT_MAX;
    trianglePso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    trianglePso.NumRenderTargets = 1;
    trianglePso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    trianglePso.SampleDesc.Count = 1;

    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&trianglePso, IID_PPV_ARGS(&g_PipelineState)));

    // quads
    ComPtr<ID3DBlob> quadVertexShader;
    shaderErrors.Reset();

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VSQuad", "vs_5_0", 0, 0, &quadVertexShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC quadPso = trianglePso;
    ComPtr<ID3DBlob> quadPixelShader;
    shaderErrors.Reset();

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PSQuad", "ps_5_0", 0, 0, &quadPixelShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    quadPso.VS = { quadVertexShader->GetBufferPointer(), quadVertexShader->GetBufferSize() };
    quadPso.PS = { quadPixelShader->GetBufferPointer(), quadPixelShader->GetBufferSize() };
    quadPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&quadPso, IID_PPV_ARGS(&g_QuadPipelineState)));

    // lines
    ComPtr<ID3DBlob> lineVertexShader;
    shaderErrors.Reset();

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "VSLine", "vs_5_0", 0, 0, &lineVertexShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC linePso = quadPso;
    ComPtr<ID3DBlob> linePixelShader;
    shaderErrors.Reset();

    hr = D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "PSLine", "ps_5_0", 0, 0, &linePixelShader, &shaderErrors);

    if (shaderErrors) {
        OutputDebugStringA(static_cast<const char*>(shaderErrors->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    linePso.VS = { lineVertexShader->GetBufferPointer(), lineVertexShader->GetBufferSize() };
    linePso.PS = { linePixelShader->GetBufferPointer(), linePixelShader->GetBufferSize() };
    linePso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&linePso, IID_PPV_ARGS(&g_LinePipelineState)));

    // stop here

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
