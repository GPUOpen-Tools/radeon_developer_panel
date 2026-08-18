// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Application class implementation

#include "application.h"

#include <algorithm>

#include <utility>

#include "utilities.h"

namespace rdp
{
    Application::Application()
        : name_("")
    {
    }

    Application::Application(QString name)
        : name_(std::move(name))
    {
    }

    Application::~Application()
    {
    }

    void Application::ClientConnected(const DDConnectionInfo& connection_info)
    {
        Q_ASSERT(connections_.empty() || connection_info.processId == connections_[0].process_id);

        const auto existing_connection = std::ranges::find_if(
            connections_, [=](const UmdConnection& umd_connection) { return umd_connection.umd_connection_id == connection_info.umdConnectionId; });

        // Duplicate UMD connection
        if (existing_connection != connections_.end())
        {
            return;
        }

        connections_.push_back({connection_info.processId, connection_info.umdConnectionId, connection_info.pDescription});
    }

    void Application::ClientDisconnected(uint32_t umd_connection_id)
    {
        const auto existing_connection =
            std::ranges::find_if(connections_, [=](const UmdConnection& umd_connection) { return umd_connection.umd_connection_id == umd_connection_id; });

        if (existing_connection == connections_.end())
        {
            return;
        }

        connections_.erase(existing_connection);
    }

    const QString& Application::GetName() const
    {
        return name_;
    }

    void Application::SetName(const QString& name)
    {
        name_ = name;
    }

    size_t Application::GetClientCount() const
    {
        return connections_.size();
    }

    QString Application::GetClientDriverDescription(int index) const
    {
        if (index < 0 || index >= static_cast<int>(connections_.size()))
        {
            return "";
        }

        return connections_[index].client_driver_description.c_str();
    }

    DDProcessId Application::GetProcessId() const
    {
        return connections_.empty() ? 0 : connections_[0].process_id;
    }

    bool Application::IsAlive() const
    {
        return GetClientCount() > 0;
    }

    bool Application::operator==(const std::shared_ptr<Application>& other) const
    {
        return this->name_ == other->name_;
    }

}  // namespace rdp
