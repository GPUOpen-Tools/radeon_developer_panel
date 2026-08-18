// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "d3d12_context.hpp"

#include <DirectXColors.h>

#include <fmt/xchar.h>

#include <fmt/format.h>

namespace
{
    void FindCompatibleAdapter(const winrt::com_ptr<IDXGIFactory1>& factory, IDXGIAdapter1** adapter)
    {
        *adapter = nullptr;

        winrt::com_ptr<IDXGIAdapter1> temp;
        if (const winrt::com_ptr<IDXGIFactory6> factory6 = factory.as<IDXGIFactory6>())
        {
            for (UINT index = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&temp))); index++)
            {
                DXGI_ADAPTER_DESC1 desc;
                winrt::check_hresult(temp->GetDesc1(&desc));

                // Ignore software devices
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

#ifdef _DEBUG
                wchar_t buffer[256];
                swprintf_s(buffer, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", index, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buffer);
#endif

                break;
            }
        }

        if (!temp)
        {
            temp = nullptr;
            for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapters1(adapterIndex, temp.put())); adapterIndex++)
            {
                DXGI_ADAPTER_DESC1 desc;
                winrt::check_hresult(temp->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

#ifdef _DEBUG
                wchar_t buff[256] = {};
                swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterIndex, desc.VendorId, desc.DeviceId, desc.Description);
                OutputDebugStringW(buff);
#endif

                break;
            }
        }

        if (!temp)
        {
            throw std::runtime_error("No Direct3D device found.");
        }

        *adapter = temp.detach();
    }
}  // namespace

D3D12Context::D3D12Context(HWND window)
    : m_backBufferFormat(DXGI_FORMAT_R16G16B16A16_FLOAT)
    , m_window(window)

{
    DWORD createFactoryFlags = 0;
#ifdef _DEBUG
    winrt::com_ptr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
    else
    {
        throw std::runtime_error("Failed to enable Direct3D Debug Layer");
    }

    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_infoQueue))))
    {
        createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

        m_infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        m_infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
    }
#endif

    winrt::check_hresult(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_factory)));
    m_factory = m_factory.as<IDXGIFactory6>();

    winrt::com_ptr<IDXGIAdapter1> adapter;
    FindCompatibleAdapter(m_factory, adapter.put());

    winrt::check_hresult(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
    m_device->SetName(L"D3D12Context::Device");

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    commandQueueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    winrt::check_hresult(m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_commandQueue)));
    winrt::check_hresult(m_commandQueue->SetName(L"D3D12Context::CommandQueue"));

    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc = {};
    rtvDescriptorHeapDesc.NumDescriptors             = FRAME_COUNT;
    rtvDescriptorHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    winrt::check_hresult(m_device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc = {};
    dsvDescriptorHeapDesc.NumDescriptors             = 1;
    dsvDescriptorHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    winrt::check_hresult(m_device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_dsvDescriptorHeap)));

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        winrt::check_hresult(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[i])));

        std::wstring name = fmt::format(L"D3D12Context::CommandAllocator{}", i);
        winrt::check_hresult(m_commandAllocator[i]->SetName(name.c_str()));

        winrt::check_hresult(m_device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_commandList[i])));

        name = fmt::format(L"D3D12Context::CommandList{}", i);
        winrt::check_hresult(m_commandList[i]->SetName(name.c_str()));

        winrt::check_hresult(m_device->CreateFence(m_frameFenceValue[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence[i])));
        m_frameFenceValue[i] = 0;

        name = fmt::format(L"D3D12Context::FrameFence{}", i);
        winrt::check_hresult(m_frameFence[i]->SetName(name.c_str()));

        m_frameFenceEvent[i].attach(CreateEventEx(nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE));
        if (!m_frameFenceEvent[i])
        {
            throw std::runtime_error("Failed to allocate frame fence event");
        }
    }

    CreateSurfaceResources();
}

D3D12Context::~D3D12Context()
{
    // Ensure swapchain is not in fullscreen state (ALT-ENTER) before releasing
    winrt::check_hresult(m_swapChain->SetFullscreenState(FALSE, nullptr));
}

void D3D12Context::BeginFrame()
{
    WaitForGpuFence(
        m_frameFence[m_currentBackBufferIndex].get(), m_frameFenceValue[m_currentBackBufferIndex], m_frameFenceEvent[m_currentBackBufferIndex].get());

    // TODO: Update ConstantBuffers here

    // Wait until all queued frames are finished
    WaitForSingleObjectEx(m_frameLatencyAwaitable.get(), INFINITE, FALSE);

    PrepareWork(m_commandAllocator[m_currentBackBufferIndex].get(), m_commandList[m_currentBackBufferIndex].get());
}

void D3D12Context::EndFrame()
{
    FinalizeWork(m_commandList[m_currentBackBufferIndex].get(), m_commandQueue.get());

    Present();
}

void D3D12Context::WaitForGpuFence(ID3D12Fence* fence, UINT64 completionValue, HANDLE fenceEvent)
{
    if (fence->GetCompletedValue() < completionValue)
    {
        winrt::check_hresult(fence->SetEventOnCompletion(completionValue, fenceEvent));

        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }
}

void D3D12Context::PrepareWork(ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList)
{
    winrt::check_hresult(commandAllocator->Reset());
    winrt::check_hresult(commandList->Reset(commandAllocator, nullptr));

    commandList->RSSetViewports(1, &m_viewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    auto barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_currentBackBufferIndex].get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrier);

    const auto rtvHandle = RenderTargetView();
    const auto dsvHandle = DepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    commandList->ClearRenderTargetView(rtvHandle, DirectX::Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void D3D12Context::FinalizeWork(ID3D12GraphicsCommandList* commandList, ID3D12CommandQueue* commandQueue)
{
    auto barrier =
        CD3DX12_RESOURCE_BARRIER::Transition(m_renderTarget[m_currentBackBufferIndex].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);
    winrt::check_hresult(commandList->Close());

    ID3D12CommandList* commandLists[] = {commandList};
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12Context::RenderTargetView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBufferIndex, m_rtvDescriptorSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE D3D12Context::DepthStencilView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Context::Present()
{
    winrt::check_hresult(m_swapChain->Present(1, 0));

    winrt::check_hresult(m_commandQueue->Signal(m_frameFence[m_currentBackBufferIndex].get(), m_currentFenceValue));
    m_frameFenceValue[m_currentBackBufferIndex] = m_currentFenceValue;
    ++m_currentFenceValue;

    m_currentBackBufferIndex = (m_currentBackBufferIndex + 1) % FRAME_COUNT;
}

void D3D12Context::CreateSurfaceResources()
{
    WaitForGpuCompletion();

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        m_renderTarget[i] = nullptr;
    }

    RECT rect;
    GetClientRect(m_window, &rect);

    const UINT width  = static_cast<UINT>(rect.right - rect.left);
    const UINT height = static_cast<UINT>(rect.bottom - rect.top);

    m_viewport    = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    m_scissorRect = CD3DX12_RECT(rect.left, rect.top, rect.right, rect.bottom);

    constexpr auto swapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    if (m_swapChain)
    {
        // TODO: Handle device lost
        m_swapChain->ResizeBuffers(FRAME_COUNT, width, height, m_backBufferFormat, swapChainFlags);
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width                 = width;
        swapChainDesc.Height                = height;
        swapChainDesc.Format                = m_backBufferFormat;
        swapChainDesc.BufferUsage           = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount           = FRAME_COUNT;
        swapChainDesc.SampleDesc.Count      = 1;
        swapChainDesc.SampleDesc.Quality    = 0;
        swapChainDesc.Scaling               = DXGI_SCALING_STRETCH;
        swapChainDesc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode             = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags                 = swapChainFlags;

        winrt::com_ptr<IDXGISwapChain1> swapChain;
        winrt::check_hresult(m_factory->CreateSwapChainForHwnd(m_commandQueue.get(), m_window, &swapChainDesc, nullptr, nullptr, swapChain.put()));
        m_swapChain = swapChain.as<IDXGISwapChain3>();

        m_frameLatencyAwaitable.attach(m_swapChain->GetFrameLatencyWaitableObject());
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < FRAME_COUNT; i++)
    {
        winrt::check_hresult(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTarget[i])));

        std::wstring name = fmt::format(L"D3D12Context::RenderTarget{}", i);
        winrt::check_hresult(m_renderTarget[i]->SetName(name.c_str()));

        D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
        renderTargetViewDesc.Format                        = m_backBufferFormat;
        renderTargetViewDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_device->CreateRenderTargetView(m_renderTarget[i].get(), &renderTargetViewDesc, rtvDescriptor);

        rtvDescriptor.Offset(1, m_rtvDescriptorSize);
    }

    m_currentBackBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

    CD3DX12_HEAP_PROPERTIES depthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 1);
    depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClear    = {};
    depthClear.Format               = DXGI_FORMAT_D32_FLOAT;
    depthClear.DepthStencil.Depth   = 1.0f;
    depthClear.DepthStencil.Stencil = 0;
    winrt::check_hresult(m_device->CreateCommittedResource(
        &depthHeapProperties, D3D12_HEAP_FLAG_NONE, &depthStencilDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClear, IID_PPV_ARGS(&m_depthStencilTarget)));
    winrt::check_hresult(m_depthStencilTarget->SetName(L"D3D12Context::DepthStencilTarget"));

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format                        = DXGI_FORMAT_D32_FLOAT;
    depthStencilViewDesc.ViewDimension                 = D3D12_DSV_DIMENSION_TEXTURE2D;

    m_device->CreateDepthStencilView(m_depthStencilTarget.get(), &depthStencilViewDesc, m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3D12Context::ResizeSwapChain()
{
    WaitForGpuCompletion();

    CreateSurfaceResources();
}

void D3D12Context::WaitForGpuCompletion()
{
    winrt::check_hresult(m_commandQueue->Signal(m_frameFence[m_currentBackBufferIndex].get(), m_currentFenceValue));
    m_frameFenceValue[m_currentBackBufferIndex] = m_currentFenceValue;
    ++m_currentFenceValue;

    WaitForGpuFence(
        m_frameFence[m_currentBackBufferIndex].get(), m_frameFenceValue[m_currentBackBufferIndex], m_frameFenceEvent[m_currentBackBufferIndex].get());
}

ID3D12Device* D3D12Context::Device() const
{
    return m_device.get();
}

ID3D12CommandQueue* D3D12Context::CommandQueue() const
{
    return m_commandQueue.get();
}

ID3D12GraphicsCommandList* D3D12Context::CommandList() const
{
    return m_commandList[m_currentBackBufferIndex].get();
}

DXGI_FORMAT D3D12Context::BackBufferFormat() const
{
    return m_backBufferFormat;
}

const D3D12Context::StaticSamplers D3D12Context::Samplers() const
{
    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(0,                                 // shaderRegister
                                                 D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
                                                 D3D12_TEXTURE_ADDRESS_MODE_WRAP,   // addressU
                                                 D3D12_TEXTURE_ADDRESS_MODE_WRAP,   // addressV
                                                 D3D12_TEXTURE_ADDRESS_MODE_WRAP);  // addressW

    return {linearWrap};
}
