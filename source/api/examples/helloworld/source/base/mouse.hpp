//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <array>

#include <SDL3/SDL.h>

/// @brief A simple Mouse class for accessing the input device state.
class Mouse final
{
    struct ButtonState
    {
        bool    IsDoubleClick;
        bool    Pressed;
        int32_t X;
        int32_t Y;
    };
    static constexpr int32_t MouseButtons = 5;
    using MouseButtonState                = std::array<ButtonState, MouseButtons>;

public:
    /// @brief Constructor
    /// @param [in] window The window used for warping mouse cursor.
    explicit Mouse(SDL_Window* window);

    /// @brief Checks if a left mouse button click has occurred.
    /// @return True if left button clicked, false otherwise.
    [[nodiscard]] bool LeftClick() const;

    /// @brief Checks if a left mouse button double-click as occurred.
    /// @return True if left button double-clicked, false otherwise.
    [[nodiscard]] bool LeftDoubleClick() const;

    /// @brief Checks if a right mouse button click has occurred.
    /// @return True if right button clicked, false otherwise.
    [[nodiscard]] bool RightClick() const;

    [[nodiscard]] bool LeftPressed() const;

    [[nodiscard]] bool RightPressed() const;

    /// @brief Checks if a right mouse button double-click has occurred.
    /// @return True if right button double-clicked, false otherwise.
    [[nodiscard]] bool RightDoubleClick() const;

    /// @brief Gets the mouse X coordinate location.
    /// @return X coordinate.
    [[nodiscard]] int32_t X() const;

    /// @brief Gets the mouse Y coordinate location.
    /// @return Y coordinate.
    [[nodiscard]] int32_t Y() const;

    /// @brief Gets the mouse relative movement in X coordinate.
    /// @return Relative mouse movement in X coordinate space.
    [[nodiscard]] int32_t RelativeX() const;

    /// @brief Gets the mouse relative movement in Y coordinate.
    /// @return Relative mouse movement in Y coordinate space.
    [[nodiscard]] int32_t RelativeY() const;

    /// @brief Gets the precise scroll-wheel movement in X axis.
    /// @return Precise scroll-wheel movement in X axis.
    [[nodiscard]] float WheelX() const;

    /// @brief Gets the precise scroll-wheel movement in Y axis.
    /// @return Precise scroll-wheel movement in Y axis.
    [[nodiscard]] float WheelY() const;

    /// @brief Constrains (warps) the mouse cursor to center of window.
    void Warp() const;

    /// @brief Registers mouse motion event.
    /// @param [in] event The mouse motion event.
    void RegisterMouseMotion(const SDL_MouseMotionEvent* event);

    /// @brief Register mouse wheel event.
    /// @param [in] event The mouse wheel event.
    void RegisterMouseWheel(const SDL_MouseWheelEvent* event);

    /// @brief Registers mouse button event.
    /// @param [in] event The mouse button event.
    void RegisterMouseButton(const SDL_MouseButtonEvent* event);

    /// @brief Updates the state cache for next frame.
    void Update();

private:
    SDL_Window*      Window_;           //< Window used to warp cursor to.
    int32_t          LocationX_{};      //< X mouse location.
    int32_t          LocationY_{};      //< Y mouse location.
    int32_t          RelativeX_{};      //< X mouse location relative to last frame.
    int32_t          RelativeY_{};      //< Y mouse location relative to last frame.
    float            PreciseWheelX_{};  //< Scroll-wheel delta X (precise).
    float            PreciseWheelY_{};  //< Scroll-wheel delta Y (precise).
    MouseButtonState CurrentState_{};   //< Current frame mouse button state.
    MouseButtonState PreviousState_{};  //< Previous frame mouse button state.
};
