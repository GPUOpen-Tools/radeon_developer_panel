// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "example.hpp"

#include <SDL3/SDL_properties.h>

#include <fmt/format.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl3.h>

namespace
{
    // Shader-visible CBV_SRV_UAV heap shared by the ImGui DX12 backend. ImGui 1.92+
    // can request several descriptors (font atlas + user textures), so a small
    // free-list allocator hands them out from a fixed-size heap.
    constexpr UINT kImGuiSrvHeapSize = 64;

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 19200
    struct ImGuiDescriptorHeapAllocator
    {
        ID3D12DescriptorHeap*       Heap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu{};
        UINT                        HeapHandleIncrement = 0;
        std::vector<int>            FreeIndices;

        void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap)
        {
            const D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
            Heap                                  = heap;
            HeapStartCpu                          = heap->GetCPUDescriptorHandleForHeapStart();
            HeapStartGpu                          = heap->GetGPUDescriptorHandleForHeapStart();
            HeapHandleIncrement                   = device->GetDescriptorHandleIncrementSize(desc.Type);
            FreeIndices.reserve(static_cast<size_t>(desc.NumDescriptors));
            for (int n = static_cast<int>(desc.NumDescriptors); n > 0; n--)
            {
                FreeIndices.push_back(n - 1);
            }
        }

        void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            assert(!FreeIndices.empty());
            const int idx = FreeIndices.back();
            FreeIndices.pop_back();
            outCpu->ptr = HeapStartCpu.ptr + static_cast<SIZE_T>(idx) * HeapHandleIncrement;
            outGpu->ptr = HeapStartGpu.ptr + static_cast<UINT64>(idx) * HeapHandleIncrement;
        }

        void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE /*gpu*/)
        {
            const int idx = static_cast<int>((cpu.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
            FreeIndices.push_back(idx);
        }
    };

    ImGuiDescriptorHeapAllocator g_imguiSrvAllocator;
#endif
}  // namespace

Example::Example(const char* title, uint32_t width, uint32_t height, const bool fullscreen)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        fprintf(stderr, "Failed to initialize SDL.\n");
        abort();
    }

    int flags = SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;
    if (fullscreen)
    {
        int        numDisplays = 0;
        const auto displays    = SDL_GetDisplays(&numDisplays);
        assert(numDisplays != 0);

        const auto mode = SDL_GetDesktopDisplayMode(displays[0]);
        width           = mode->w;
        height          = mode->h;
        SDL_free(displays);

        flags |= SDL_WINDOW_FULLSCREEN;
    }

    m_window = SDL_CreateWindow(title, static_cast<int>(width), static_cast<int>(height), flags);
    if (!m_window)
    {
        fprintf(stderr, "Failed to create SDL window.\n");
        abort();
    }
    m_running = true;

    auto hwnd  = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(m_window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    m_context  = std::make_unique<D3D12Context>(hwnd);
    m_keyboard = std::make_unique<Keyboard>();
    m_mouse    = std::make_unique<Mouse>(m_window);

    // Timer.SetTargetElapsedSeconds(1.0f / static_cast<float>(mode.refresh_rate));
    m_timer.SetFixedTimeStep(false);

    const auto      actualWidth  = GetFrameWidth();
    const auto      actualHeight = GetFrameHeight();
    const float     aspect       = static_cast<float>(actualWidth) / static_cast<float>(actualHeight);
    constexpr float fov          = XMConvertToRadians(75.0f);
    constexpr float nearPlane    = 0.01f;
    constexpr float farPlane     = 1000.0f;

    m_camera = std::make_unique<Camera>(Vector3::Zero, Vector3::Forward, Vector3::Up, fov, aspect, nearPlane, farPlane, 800.0f, 600.0f);

    // Dear ImGui: shared shader-visible SRV heap, then platform + renderer backends.
    D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
    imguiHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    imguiHeapDesc.NumDescriptors             = kImGuiSrvHeapSize;
    imguiHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    winrt::check_hresult(m_context->Device()->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(&m_imguiSrvHeap)));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // High-DPI and remote-desktop displays report a content scale > 1. Because the
    // window uses SDL_WINDOW_HIGH_PIXEL_DENSITY the back buffer is sized in physical
    // pixels, so scale the ImGui style and fonts to keep the UI a usable size.
    const float uiScale = SDL_GetWindowDisplayScale(m_window);
    if (uiScale > 0.0f)
    {
        ImGui::GetStyle().ScaleAllSizes(uiScale);
        ImGui::GetIO().FontGlobalScale = uiScale;
    }

    ImGui_ImplSDL3_InitForD3D(m_window);

#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 19200
    g_imguiSrvAllocator.Create(m_context->Device(), m_imguiSrvHeap.get());

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device                  = m_context->Device();
    initInfo.CommandQueue            = m_context->CommandQueue();
    initInfo.NumFramesInFlight       = FRAME_COUNT;
    initInfo.RTVFormat               = m_context->BackBufferFormat();
    initInfo.SrvDescriptorHeap       = m_imguiSrvHeap.get();
    initInfo.SrvDescriptorAllocFn    = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
        g_imguiSrvAllocator.Alloc(outCpu, outGpu);
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
        g_imguiSrvAllocator.Free(cpu, gpu);
    };
    ImGui_ImplDX12_Init(&initInfo);
#else
    ImGui_ImplDX12_Init(m_context->Device(),
                        FRAME_COUNT,
                        m_context->BackBufferFormat(),
                        m_imguiSrvHeap.get(),
                        m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
                        m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart());
#endif
}

Example::~Example()
{
    // Make sure the GPU is done with any in-flight ImGui draws before tearing down.
    m_context->WaitForGpuCompletion();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (m_window != nullptr)
    {
        SDL_DestroyWindow(m_window);
    }
    SDL_Quit();
}

uint32_t Example::GetFrameWidth() const
{
    int32_t w;
    SDL_GetWindowSizeInPixels(m_window, &w, nullptr);
    return w;
}

uint32_t Example::GetFrameHeight() const
{
    int32_t h;
    SDL_GetWindowSizeInPixels(m_window, nullptr, &h);
    return h;
}

int Example::Run([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    if (!Load())
    {
        return EXIT_FAILURE;
    }

    while (m_running)
    {
        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL3_ProcessEvent(&e);

            if (e.type == SDL_EVENT_QUIT)
            {
                m_running = false;
                continue;
            }

            if (e.type == SDL_EVENT_WINDOW_RESIZED)
            {
                m_context->ResizeSwapChain();

                const auto      actualWidth  = GetFrameWidth();
                const auto      actualHeight = GetFrameHeight();
                const float     aspect       = static_cast<float>(actualWidth) / static_cast<float>(actualHeight);
                constexpr float fov          = XMConvertToRadians(75.0f);
                constexpr float nearPlane    = 0.01f;
                constexpr float farPlane     = 1000.0f;
                m_camera->setProjection(fov, aspect, nearPlane, farPlane, actualWidth, actualHeight);
            }

            if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP)
            {
                m_keyboard->RegisterKeyEvent(&e.key);
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            {
                m_mouse->RegisterMouseButton(&e.button);
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION)
            {
                m_mouse->RegisterMouseMotion(&e.motion);
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL)
            {
                m_mouse->RegisterMouseWheel(&e.wheel);
            }
        }

        const auto elapsed = static_cast<float>(m_timer.GetElapsedSeconds());
        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_LSHIFT) && m_mouse->LeftPressed() && m_mouse->RightPressed())
        {
            m_camera->moveForward(elapsed * m_mouse->RelativeY());
        }

        if (m_keyboard->IsKeyClicked(SDL_SCANCODE_ESCAPE))
        {
            Quit();
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_W))
        {
            m_camera->moveForward(elapsed);
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_S))
        {
            m_camera->moveBackward(elapsed);
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_A))
        {
            m_camera->strafeLeft(elapsed);
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_D))
        {
            m_camera->strafeRight(elapsed);
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_LEFT))
        {
            m_camera->rotate(0.0f, elapsed);
        }

        if (m_keyboard->IsKeyPressed(SDL_SCANCODE_RIGHT))
        {
            m_camera->rotate(0.0f, -elapsed);
        }

        m_timer.Tick([this]() { Update(m_timer); });

        m_context->BeginFrame();

        const auto commandList = m_context->CommandList();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Render(commandList, m_timer);

        OnGui();

        ImGui::Render();
        ID3D12DescriptorHeap* imguiHeaps[] = {m_imguiSrvHeap.get()};
        commandList->SetDescriptorHeaps(_countof(imguiHeaps), imguiHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

        m_context->EndFrame();

        m_keyboard->Update();
        m_mouse->Update();
    }

    m_context->WaitForGpuCompletion();

    return 0;
}

void Example::Quit()
{
    m_running = false;
}
