// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash reporting factory declarations.

#ifndef RDP_SOURCE_FRONTEND_CRASH_CRASH_FACTORY_H_
#define RDP_SOURCE_FRONTEND_CRASH_CRASH_FACTORY_H_

#include <memory>

#include <QtGlobal>

#include "crash_handler.h"
#include "crash_notify.h"

#ifdef Q_OS_WIN
#include "win/crash_handler_win.h"
#include "win/crash_notify_win.h"
#endif

namespace rdp
{
    /// @brief Factory for creating crash notifiers.
    struct CrashNotifierFactory
    {
        /// @brief Delete the default constructor.
        CrashNotifierFactory() = delete;

        /// @brief Creates a new crash notifier for the correct operating system.
        /// @return A new crash notifier.
        static std::unique_ptr<CrashNotifier> Create()
        {
            std::unique_ptr<CrashNotifier> out;

#ifdef Q_OS_WIN
            out = std::make_unique<CrashNotifierWin>();
#endif

            return out;
        }
    };

    /// @brief Factory for creating crash handlers.
    struct CrashHandlerFactory
    {
        /// @brief Delete the default constructor.
        CrashHandlerFactory() = delete;

        /// @brief Creates a new crash handler for the correct operating system.
        /// @return A new crash notifier.
        static std::unique_ptr<CrashHandler> Create(const std::shared_ptr<class SystemInfoModel>& system_info_model)
        {
            std::unique_ptr<CrashHandler> out;

#ifdef Q_OS_WIN
            out = std::make_unique<CrashHandlerWin>(system_info_model);
#else
            // By default, use a crash handler with no actual implementation to make it easier to
            // pass around the handler without having to check for nullptr everywhere.
            out.reset(new CrashHandler(system_info_model));
#endif

            return out;
        }
    };

}  // namespace rdp

#endif
