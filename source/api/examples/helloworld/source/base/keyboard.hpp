//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <map>

#include <SDL3/SDL.h>

class Keyboard final
{
    using KeyState = std::map<SDL_Scancode, bool>;

public:
    /// @brief Checks if specified key was clicked this frame.
    /// @param [in] key The clicked key.
    /// @return True if key was clicked, false otherwise.
    bool IsKeyClicked(SDL_Scancode key);

    /// @brief Checks if specified key is pressed this frame.
    /// @param [in] key The pressed key.
    /// @return True if pressed, false otherwise.
    bool IsKeyPressed(SDL_Scancode key);

    /// @brief Registers key event.
    /// @param [in] event The key event.
    void RegisterKeyEvent(const SDL_KeyboardEvent* event);

    /// @brief Updates the state cache for next frame.
    void Update();

private:
    KeyState PreviousKeyState_{};  ///< Previous frame key-state.
    KeyState CurrentKeyState_{};   ///< Current frame key-state.
};
