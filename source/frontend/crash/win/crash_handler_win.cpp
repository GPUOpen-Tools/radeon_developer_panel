// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP crash handler Windows implementation.

#include "crash_handler_win.h"

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QtGlobal>

#include <Dbghelp.h>
#include <windows.h>

namespace rdp
{
    /// @brief The error message displayed when RDP crashes and a report could not be generated.
    static constexpr char const* kFailedMessage = "Radeon Developer Panel crashed, but a crash report could not be generated.";

    /// @brief The title for any error boxes shown to the user.
    static constexpr char const* kFailedTitle = "Crashed";

    /// @brief The name on disk of the minidump file.
    static constexpr char const* kFilename = "dump.dmp";

    /// @brief Signature of the write minidump file function that gets extracted from dbghelp.dll.
    using MinidumpWriteDump = BOOL(WINAPI*)(HANDLE                            process,
                                            DWORD                             pid,
                                            HANDLE                            file,
                                            MINIDUMP_TYPE                     dump_type,
                                            PMINIDUMP_EXCEPTION_INFORMATION   exception_data,
                                            PMINIDUMP_USER_STREAM_INFORMATION usr_stream_info,
                                            PMINIDUMP_CALLBACK_INFORMATION    callback);

    CrashHandlerWin::CrashHandlerWin(const std::shared_ptr<class SystemInfoModel>& system_info_model)
        : CrashHandler(system_info_model)
    {
    }

    void CrashHandlerWin::Handle(ExtendedCrashInfo& crash_info)
    {
        // Dynamically load the module in case the program was corrupted. If there is program corruption, the minidump could be invalid anyway,
        // but dynamic loading mitigates this issues somewhat.
        HMODULE dbghelp_module = LoadLibrary(TEXT("dbghelp.dll"));
        if (dbghelp_module == nullptr)
        {
            SignalHandleFailure();
            return;
        }

        // Get the MiniDumpWriteDump function from the DLL
        auto minidump_writer = reinterpret_cast<MinidumpWriteDump>(GetProcAddress(dbghelp_module, "MiniDumpWriteDump"));
        if (minidump_writer == nullptr)
        {
            SignalHandleFailure();
            return;
        }

        QString output_path = QDir::toNativeSeparators(crash_info.output_directory.filePath(kFilename));
        HANDLE  out_file = CreateFileW(qUtf16Printable(output_path), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (out_file == INVALID_HANDLE_VALUE)
        {
            CloseHandle(out_file);
            FreeLibrary(dbghelp_module);

            SignalHandleFailure();
            return;
        }

        MINIDUMP_EXCEPTION_INFORMATION exception_info;
        exception_info.ThreadId          = GetCurrentThreadId();
        exception_info.ExceptionPointers = crash_info.os_info.exception_data;
        exception_info.ClientPointers    = FALSE;

        MINIDUMP_TYPE type         = static_cast<MINIDUMP_TYPE>(MiniDumpWithPrivateReadWriteMemory | MiniDumpWithDataSegs | MiniDumpWithHandleData |
                                                        MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
        BOOL          dump_written = minidump_writer(GetCurrentProcess(), GetCurrentProcessId(), out_file, type, &exception_info, nullptr, nullptr);

        if (static_cast<bool>(dump_written))
        {
            QString crash_message =
                QString("Radeon Developer Panel crashed! Crash information was written to %1").arg(crash_info.output_directory.absolutePath());
            MessageBoxA(nullptr, qPrintable(crash_message), kFailedTitle, MB_DEFBUTTON1);
        }
        else
        {
            SignalHandleFailure();
        }

        CloseHandle(out_file);
        FreeLibrary(dbghelp_module);
    }

    void CrashHandlerWin::SignalHandleFailure()
    {
        MessageBoxA(nullptr, kFailedMessage, kFailedTitle, MB_DEFBUTTON1);
    }

}  // namespace rdp
