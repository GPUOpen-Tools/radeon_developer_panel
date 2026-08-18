// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash notifier declaration.

#ifndef RDP_SOURCE_FRONTEND_CRASH_CRASH_NOTIFY_H_
#define RDP_SOURCE_FRONTEND_CRASH_CRASH_NOTIFY_H_

#include <functional>

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>

#include <errhandlingapi.h>
#endif

namespace rdp
{
    /// @brief Contains OS specific information about a crash.
    struct CrashInfo
    {
#ifdef Q_OS_WIN
        /// @brief The information about the exception that has occurred.
        EXCEPTION_POINTERS* exception_data;
#endif
    };

    /// @brief Notifies a handler when an unhandled exception has occurred somewhere in the application.
    class CrashNotifier
    {
    public:
        /// @brief Destructor.
        virtual ~CrashNotifier() = default;

        /// @brief Registers the notifier to handle crashes.
        virtual void Register() = 0;

        /// @brief Sets the handler for when a crash occurs.
        /// @param new_handler The new handler for application crashes.
        virtual void SetCrashHandler(std::function<void(const CrashInfo&)> new_handler) = 0;
    };

}  // namespace rdp

#endif
