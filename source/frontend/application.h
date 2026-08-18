// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Application class definition

#ifndef RDP_SOURCE_FRONTEND_APPLICATION_H_
#define RDP_SOURCE_FRONTEND_APPLICATION_H_

#include <array>
#include <vector>

#include <QMap>
#include <QObject>
#include <QWidget>

#include "models/api_model.h"
#include "models/connection_model.h"
#include "utilities.h"

namespace rdp
{
    /// @brief Represents a client application that may connect to RDP
    class Application : public QObject
    {
        /// @brief A unique connection to the UMD.
        struct UmdConnection
        {
            uint32_t    process_id                = 0;   ///< The process ID of the connection.
            uint32_t    umd_connection_id         = 0;   ///< The ID of the connection.
            std::string client_driver_description = "";  ///< The description of the connection.
        };

        Q_OBJECT
    public:
        /// @brief Default constructor
        /// @param connection_model The model that manages connections to applications.
        explicit Application();

        /// @brief Constructs an application with specified name and workflow.
        /// @param [in] name The application name.
        /// @param [in] connection_model The model that manages connections to applications.
        explicit Application(QString name);

        /// @brief Destructor
        ~Application() Q_DECL_OVERRIDE;

        /// @brief Should be called when a new client connects for the application.
        /// @param [in] connection_info The information about the new connection.
        void ClientConnected(const DDConnectionInfo& connection_info);

        /// @brief Should be called when a client disconnects for the application.
        /// @param [in] umd_connection_id The identifier of the connection.
        void ClientDisconnected(uint32_t umd_connection_id);

        /// @brief Gets the name of entry
        /// @return entry name
        const QString& GetName() const;

        /// @brief Sets the name of entry
        /// @param [in] name The name
        void SetName(const QString& name);

        /// @brief Gets the number of active clients
        /// @return number of clients
        size_t GetClientCount() const;

        /// @brief Gets the driver description for client
        /// @param [in] index The index of client
        /// @return client driver description
        QString GetClientDriverDescription(int index) const;

        /// @brief Gets the process ID for associated client
        /// @return process ID or 0 if no client
        DDProcessId GetProcessId() const;

        /// @brief Gets alive state of entry
        /// @return true if entry has active client context
        bool IsAlive() const;

        /// @brief Compares application against another instance
        /// @param other The other application to compare against
        /// @return true if equal false otherwise
        bool operator==(const std::shared_ptr<Application>& other) const;

    private:
        QString                    name_;         ///< Application name
        std::vector<UmdConnection> connections_;  ///< The current connections for the application.
    };

}  // namespace rdp

#endif
