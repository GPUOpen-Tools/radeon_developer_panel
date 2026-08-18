// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash handler declaration.

#ifndef RDP_SOURCE_FRONTEND_CRASH_CRASH_HANDLER_H_
#define RDP_SOURCE_FRONTEND_CRASH_CRASH_HANDLER_H_

#include <memory>

#include <QDir>
#include <QObject>
#include <QString>

#include "crash_notify.h"

namespace rdp
{
    /// @brief Extended crash information.
    struct ExtendedCrashInfo
    {
        /// @brief Constructor.
        /// @param crash_info OS specific information about a crash.
        explicit ExtendedCrashInfo(const CrashInfo& crash_info)
            : os_info(crash_info)
        {
        }

        const CrashInfo& os_info;           ///< OS specific information about a crash.
        QDir             output_directory;  ///< The output path on disk for any crash information.
    };

    /// @brief Handles a crash.
    class CrashHandler : public QObject
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] system_info_model The model that manages the system info.
        CrashHandler(const std::shared_ptr<class SystemInfoModel>& system_info_model);

        /// @brief Destructor.
        ~CrashHandler() override = default;

        /// @brief Handles a crash by doing something like generating a minidump file.
        /// @param info The information about the crash.
        void Handle(const CrashInfo& info);

    protected:
        /// @brief Handles a crash by doing something like generating a minidump file.
        ///
        /// Should be overridden by a child class to provide an OS specific implementation.
        /// @param info The information about the crash with the location to write files to.
        virtual void Handle(ExtendedCrashInfo& crash_info);

    public slots:

        /// @brief Called when the system information changes.
        ///
        /// This should be called with empty when RDS is disconnected.
        /// @paran new_system_info The JSON string for the new system information.
        void SystemInfoChanged(const QString& system_info_json);

    private:
        QString system_info_json_;  ///< The system information as a JSON string.
    };

}  // namespace rdp

#endif
