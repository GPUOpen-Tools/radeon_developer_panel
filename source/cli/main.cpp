// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Main entry point for RadeonDeveloperPanelCLI.

#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <csignal>
#endif

#include "capture_cli.h"

namespace
{
    CaptureCli* g_app_instance = nullptr;

#ifdef _WIN32
    BOOL WINAPI ConsoleCtrlHandler(const DWORD ctrl_type)
    {
        switch (ctrl_type)
        {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            // Just set the interrupt flag - cleanup happens in main thread
            if (g_app_instance != nullptr)
            {
                g_app_instance->Interrupt();
            }
            return TRUE;
        default:
            return FALSE;
        }
    }
#else
    void SignalHandler(int signal)
    {
        (void)signal;
        // Just set the interrupt flag - cleanup happens in main thread
        if (g_app_instance != nullptr)
        {
            g_app_instance->Interrupt();
        }
    }
#endif
}  // namespace

int main(const int argc, char* argv[])
{
#ifdef _WIN32
    // Enable UTF-8 output so driver-provided strings (experiment descriptions,
    // etc.) render correctly instead of producing mojibake like "ΓÇÖ".
    SetConsoleOutputCP(CP_UTF8);

    // Set up console control handler for Ctrl+C on Windows
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#else
    // Set up signal handler for Ctrl+C on Unix
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#endif

    CaptureConfig config{};
    if (!ParseCommandLine(argc, argv, config))
    {
        return 1;
    }

    CaptureCli capture_cli(std::move(config));
    g_app_instance = &capture_cli;

    const int result = capture_cli.Run();

    g_app_instance = nullptr;
    return result;
}
