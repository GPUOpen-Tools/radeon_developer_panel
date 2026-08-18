// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "mouse.hpp"

Mouse::Mouse(SDL_Window* window)
    : Window_(window)
{
    // Enable relative mouse location tracking for deltas
    // SDL_SetWindowRelativeMouseMode()
    // SDL_SetRelativeMouseMode(SDL_TRUE);
}

bool Mouse::LeftClick() const
{
    return !CurrentState_[SDL_BUTTON_LEFT].Pressed && PreviousState_[SDL_BUTTON_LEFT].Pressed;
}

bool Mouse::LeftDoubleClick() const
{
    return (CurrentState_[SDL_BUTTON_LEFT].Pressed && CurrentState_[SDL_BUTTON_LEFT].IsDoubleClick);
}

bool Mouse::LeftPressed() const
{
    return CurrentState_[SDL_BUTTON_LEFT].Pressed;
}

bool Mouse::RightClick() const
{
    return !CurrentState_[SDL_BUTTON_RIGHT].Pressed && PreviousState_[SDL_BUTTON_RIGHT].Pressed;
}

bool Mouse::RightPressed() const
{
    return CurrentState_[SDL_BUTTON_RIGHT].Pressed;
}

bool Mouse::RightDoubleClick() const
{
    return (CurrentState_[SDL_BUTTON_RIGHT].Pressed && CurrentState_[SDL_BUTTON_RIGHT].IsDoubleClick);
}

int32_t Mouse::X() const
{
    return LocationX_;
}

int32_t Mouse::Y() const
{
    int32_t w, h;
    SDL_GetWindowSize(Window_, &w, &h);

    return h - LocationY_;
    // return LocationY;
}

int32_t Mouse::RelativeX() const
{
    return RelativeX_;
}

int32_t Mouse::RelativeY() const
{
    return RelativeY_;
}

float Mouse::WheelX() const
{
    return PreciseWheelX_;
}

float Mouse::WheelY() const
{
    return PreciseWheelY_;
}

void Mouse::Warp() const
{
    int32_t w, h;
    SDL_GetWindowSize(Window_, &w, &h);
    SDL_WarpMouseInWindow(Window_, w / 2, h / 2);
}

void Mouse::RegisterMouseMotion(const SDL_MouseMotionEvent* event)
{
    LocationX_ = event->x;
    LocationY_ = event->y;
    RelativeX_ = event->xrel;
    RelativeY_ = event->yrel;
}

void Mouse::RegisterMouseWheel(const SDL_MouseWheelEvent* event)
{
    PreciseWheelX_ = event->x;
    PreciseWheelY_ = event->y;
}

void Mouse::RegisterMouseButton(const SDL_MouseButtonEvent* event)
{
    CurrentState_[event->button] = {
        .IsDoubleClick = event->clicks > 1, .Pressed = event->down, .X = static_cast<int32_t>(event->x), .Y = static_cast<int32_t>(event->y)};
}

void Mouse::Update()
{
    PreviousState_ = CurrentState_;
    // CurrentState = {};
    RelativeX_     = 0;
    RelativeY_     = 0;
    PreciseWheelX_ = 0.0f;
    PreciseWheelY_ = 0.0f;
}
