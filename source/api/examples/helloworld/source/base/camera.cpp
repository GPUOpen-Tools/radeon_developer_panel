// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "camera.hpp"

Camera::Camera(Vector3 position,
               Vector3 direction,
               Vector3 up,
               float   fov,
               float   aspectRatio,
               float   nearPlane,
               float   farPlane,
               float   viewWidth,
               float   viewHeight)
    : m_position(position)
    , m_direction(direction)
    , m_fieldOfView(fov)
    , m_aspectRatio(aspectRatio)
    , m_nearPlane(nearPlane)
    , m_farPlane(farPlane)
    , m_viewWidth(viewWidth)
    , m_viewHeight(viewHeight)
{
    updateUniforms();
}

void Camera::setProjection(const float fov, const float aspectRatio, const float nearPlane, const float farPlane, const float viewWidth, const float viewHeight)
{
    m_fieldOfView = fov;
    m_aspectRatio = aspectRatio;
    m_nearPlane   = nearPlane;
    m_farPlane    = farPlane;
    m_viewWidth   = viewWidth;
    m_viewHeight  = viewHeight;
    updateUniforms();
}

Matrix Camera::viewProjection() const
{
    return m_view * m_projection;
}

float Camera::viewWidth() const
{
    return m_viewWidth;
}

float Camera::viewHeight() const
{
    return m_viewHeight;
}

void Camera::moveForward(const float dt)
{
    Vector3 forward = direction();
    forward.y       = 0.0f;  // Lock movement to the x and z axes
    forward.Normalize();
    m_position += forward * dt * m_speed;
    updateUniforms();
}

void Camera::moveBackward(const float dt)
{
    Vector3 backward = direction();
    backward.y       = 0.0f;  // Lock movement to the x and z axes
    backward.Normalize();
    m_position -= backward * dt * m_speed;
    updateUniforms();
}

void Camera::strafeLeft(const float dt)
{
    auto _right = right();
    _right.y    = 0.0f;
    m_position -= _right * dt * m_speed;
    updateUniforms();
}

void Camera::strafeRight(const float dt)
{
    auto _right = right();
    _right.y    = 0.0f;
    m_position += _right * dt * m_speed;
    updateUniforms();
}

void Camera::setPosition(const Vector3& position)
{
    m_position = position;
    updateUniforms();
}

void Camera::setRotation(const Vector3& rotation)
{
    m_rotation = rotation;
    updateUniforms();
}

void Camera::rotate(const float pitch, const float yaw)
{
    m_orientation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, 0.0f);
    m_orientation.Normalize();

    updateUniforms();
}

Vector3 Camera::direction() const
{
    auto dir = Vector3::Transform(Vector3::Forward, m_orientation);
    dir.Normalize();
    return dir;
}

Vector3 Camera::right() const
{
    auto right = Vector3::Transform(Vector3::Right, m_orientation);
    right.Normalize();
    return right;
}

void Camera::updateUniforms()
{
    m_view           = Matrix::CreateLookAt(m_position, m_position + direction(), Vector3::Up);
    m_projection     = Matrix::CreatePerspectiveFieldOfView(m_fieldOfView, m_aspectRatio, m_nearPlane, m_farPlane);
    m_viewProjection = m_view * m_projection;
}
