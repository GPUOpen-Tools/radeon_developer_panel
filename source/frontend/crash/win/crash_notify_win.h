// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash notifier Windows declaration.

#ifndef RDP_SOURCE_FRONTEND_CRASH_WIN_CRASH_NOTIFY_WIN_H_
#define RDP_SOURCE_FRONTEND_CRASH_WIN_CRASH_NOTIFY_WIN_H_

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <errhandlingapi.h>

#include <QtGlobal>

#include "../crash_notify.h"

namespace rdp
{
    class CrashNotifierWin : public CrashNotifier
    {
    public:
        /// @brief Destructor.
        ~CrashNotifierWin() override = default;

        /// @brief Registers the notifier to handle crashes.
        void Register() override;

        /// @brief Sets the handler for when a crash occurs.
        /// @param new_handler The new handler for application crashes.
        void SetCrashHandler(std::function<void(const CrashInfo&)> new_handler) override;
    };

}  // namespace rdp

#endif
