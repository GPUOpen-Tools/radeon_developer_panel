// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP ddTool type wrapper definitions

#ifndef RDP_SOURCE_FRONTEND_TOOL_WRAPPER_TYPES_H_
#define RDP_SOURCE_FRONTEND_TOOL_WRAPPER_TYPES_H_

#include <QMetaType>
#include <QString>
#include <QVariant>

#include <MercuryModuleExt.h>

namespace rdp
{
    /// @brief Local connection info
    struct LocalConnectionInfo
    {
        // Get a description of the connection that's suitable for logging
        static QString GetDescription()
        {
            return "Local";
        }
    };

    /// @brief Remote connection info
    struct RemoteConnectionInfo
    {
        QString hostname;  // Remote hostname. e.g. "localhost" or "MY-TEST-MACHINE"
        QString nickname;  // Nickname for the connection.
        int     port = 0;  // Remote port name

        // Get a description of the connection that's suitable for logging
        QString GetDescription() const
        {
            return nickname.length() ? QString("Remote %1:%2 (%3)").arg(hostname).arg(port).arg(nickname) : GetDescriptionsSansNickname();
        }

        // Gets a description of the connection
        QString GetDescriptionsSansNickname() const
        {
            return QString("Remote %1:%2").arg(hostname).arg(port);
        }
    };

    /// @brief Enumeration for connection types
    enum class ConnectionType
    {
        kUnknown = 0,  ///< An invalid or unknown type
        kLocal,        ///< A connection over a local message bus
        kRemote,       ///< A connection to a remote machine
    };

}  // namespace rdp

// These must appear in global namespace
Q_DECLARE_METATYPE(rdp::LocalConnectionInfo)
Q_DECLARE_METATYPE(rdp::RemoteConnectionInfo)

#endif
