//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <string>

#include "camera.hpp"
#include "d3d12_context.hpp"
#include "game_timer.hpp"
#include "keyboard.hpp"
#include "mouse.hpp"

class Example
{
public:
    explicit Example(const char* title, uint32_t width, uint32_t height, bool fullscreen = false);

    virtual ~Example();

    int Run([[maybe_unused]] int argc, [[maybe_unused]] char** argv);

    void Quit();

    [[nodiscard]] uint32_t GetFrameWidth() const;

    [[nodiscard]] uint32_t GetFrameHeight() const;

    virtual bool Load() = 0;

    virtual void Update(const GameTimer& timer) = 0;

    virtual void Render(ID3D12GraphicsCommandList* commandList, const GameTimer& timer) = 0;

    // Override to record Dear ImGui widgets. Called each frame between NewFrame
    // and Render, after the derived Render. Default is a no-op.
    virtual void OnGui()
    {
    }

protected:
    static constexpr int FRAME_COUNT = 3;

    SDL_Window*                   m_window;
    std::unique_ptr<Camera>       m_camera;
    std::unique_ptr<Keyboard>     m_keyboard;
    std::unique_ptr<Mouse>        m_mouse;
    std::unique_ptr<D3D12Context> m_context;

private:
    GameTimer                            m_timer;
    bool                                 m_running;
    winrt::com_ptr<ID3D12DescriptorHeap> m_imguiSrvHeap;
};
