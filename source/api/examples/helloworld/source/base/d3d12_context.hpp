//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <array>
#include <cstdint>

#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>
#ifdef _DEBUG
#include <dxgidebug.h>
#endif
#include <dstorage.h>

class D3D12Context final
{
    static constexpr UINT FRAME_COUNT = 3;

public:
    using StaticSamplers = std::array<const CD3DX12_STATIC_SAMPLER_DESC, 1>;

    explicit D3D12Context(HWND window);

    ~D3D12Context();

    ID3D12Device*                 Device() const;
    ID3D12CommandQueue*           CommandQueue() const;
    ID3D12GraphicsCommandList*    CommandList() const;
    ID3D12Resource*               RenderTarget() const;
    ID3D12Resource*               DepthStencilTarget() const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTargetView() const;
    CD3DX12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    DXGI_FORMAT                   BackBufferFormat() const;
    const StaticSamplers          Samplers() const;

    void BeginFrame();

    void EndFrame();

    void Present();

    void WaitForGpuCompletion();

    void ResizeSwapChain();

private:
    void PrepareWork(ID3D12CommandAllocator* commandAllocator, ID3D12GraphicsCommandList* commandList);

    void FinalizeWork(ID3D12GraphicsCommandList* commandList, ID3D12CommandQueue* commandQueue);

    void WaitForGpuFence(ID3D12Fence* fence, UINT64 completionValue, HANDLE fenceEvent);

    void CreateSurfaceResources();

    DXGI_FORMAT                               m_backBufferFormat;
    HWND                                      m_window;
    CD3DX12_VIEWPORT                          m_viewport;
    CD3DX12_RECT                              m_scissorRect;
    winrt::com_ptr<ID3D12Device9>             m_device;
    winrt::com_ptr<ID3D12CommandQueue>        m_commandQueue;
    winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList[FRAME_COUNT];
    winrt::com_ptr<ID3D12CommandAllocator>    m_commandAllocator[FRAME_COUNT];
    winrt::com_ptr<IDXGIFactory6>             m_factory;
    winrt::com_ptr<IDXGISwapChain3>           m_swapChain;
    winrt::com_ptr<ID3D12Resource>            m_renderTarget[FRAME_COUNT];
    winrt::com_ptr<ID3D12Resource>            m_depthStencilTarget;
    winrt::com_ptr<ID3D12DescriptorHeap>      m_rtvDescriptorHeap;
    UINT                                      m_rtvDescriptorSize;
    winrt::com_ptr<ID3D12DescriptorHeap>      m_dsvDescriptorHeap;
#ifdef _DEBUG
    winrt::com_ptr<IDXGIInfoQueue> m_infoQueue;
#endif
    winrt::handle               m_frameLatencyAwaitable;
    winrt::com_ptr<ID3D12Fence> m_frameFence[FRAME_COUNT];
    winrt::handle               m_frameFenceEvent[FRAME_COUNT];
    UINT64                      m_frameFenceValue[FRAME_COUNT]{0};
    UINT64                      m_currentFenceValue      = 1;
    UINT                        m_currentBackBufferIndex = 0;
};
