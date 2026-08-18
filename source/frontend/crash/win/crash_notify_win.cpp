// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash notifier Windows implementation.

#include "crash_notify_win.h"

#include <csignal>
#include <mutex>

#include <windows.h>

#include <windef.h>

namespace rdp
{
    static std::function<void(const CrashInfo&)> g_crash_handler;         ///< Global crash handler callback.
    static bool                                  has_registered = false;  ///< Global registration status for handling crashes.
    static std::mutex crash_mutex;  ///< Mutex to make sure that if multiple threads are crashing at the same time, only one dumps at a time.

    /// @brief Windows system callback to handle exceptions
    static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exception_data = nullptr)
    {
        std::lock_guard<std::mutex> lock(crash_mutex);

        CrashInfo crash_info{};
        crash_info.exception_data = exception_data;

        if (g_crash_handler)
        {
            g_crash_handler(crash_info);
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }

    /// @brief Handler for SIGABRT.
    ///
    /// Things like assert() will call abort() and not get routed through UnhandledExceptionFilter. This
    /// handler will catch all of those cases since they are also considered crashes.
    static void AbortSignalHandler(int sig)
    {
        Q_UNUSED(sig)

        // If another thread also hits abort the handler will be lost, so reset it.
        signal(SIGABRT, AbortSignalHandler);
        MessageBoxW(nullptr, L"Sig abort", L"Crash", MB_DEFBUTTON1);

        // Unfortunately at this point the exception data is not available. The best we can do is to generate some
        // fake exception data. This will point to RtlCaptureContext(), but the stacktrace should contain the offending
        // RDP code in it.
        CONTEXT context;
        RtlCaptureContext(&context);

        EXCEPTION_RECORD record = {};
        record.ExceptionCode    = STATUS_FATAL_APP_EXIT;
        record.ExceptionFlags   = EXCEPTION_NONCONTINUABLE;

        // Since we only support 64-bit builds for RDP, we can get away with always using %rip as the instruction counter.
        // If we were ever to enable 32-bit RDP builds, we would need to switch between %rip and %eip.
        record.ExceptionAddress = reinterpret_cast<void*>(context.Rip);  // NOLINT(performance-no-int-to-ptr)

        EXCEPTION_POINTERS exception_pointers;
        exception_pointers.ContextRecord   = &context;
        exception_pointers.ExceptionRecord = &record;

        rdp::UnhandledExceptionFilter(&exception_pointers);
    }

    void CrashNotifierWin::Register()
    {
#ifndef _DEBUG
        if (!has_registered)
        {
            has_registered = true;

            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

            SetUnhandledExceptionFilter(UnhandledExceptionFilter);
            signal(SIGABRT, AbortSignalHandler);

            // This ensures that message boxes are not shown when abort is called
            _set_abort_behavior(0, _WRITE_ABORT_MSG);
        }
#endif
    }

    void CrashNotifierWin::SetCrashHandler(std::function<void(const CrashInfo&)> new_handler)
    {
        g_crash_handler = std::move(new_handler);
    }

}  // namespace rdp
