// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash handler Windows declaration.

#ifndef RDP_SOURCE_FRONTEND_CRASH_WIN_CRASH_HANDLER_WIN_H_
#define RDP_SOURCE_FRONTEND_CRASH_WIN_CRASH_HANDLER_WIN_H_

#include <memory>

#include <QtGlobal>

#include "../crash_handler.h"

namespace rdp
{
    /// @brief Handles a crash by generating a minidump file.
    class CrashHandlerWin : public CrashHandler
    {
    public:
        /// @brief Constructor.
        /// @param [in] system_info_model The model that manages the system info.
        CrashHandlerWin(const std::shared_ptr<class SystemInfoModel>& system_info_model);

        /// @brief Destructor.
        ~CrashHandlerWin() override = default;

        /// @brief Dumps a minidump file that contains the information about the crash.
        /// @param info The information about the crash.
        void Handle(ExtendedCrashInfo& crash_info) override;

    private:
        /// @brief Displays a message that there was an issue generating the crash report.
        static void SignalHandleFailure();
    };

}  // namespace rdp

#endif
