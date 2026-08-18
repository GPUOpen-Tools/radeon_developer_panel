// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Main entry point for Radeon Developer Service CLI

#include <iostream>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#endif

#include <ddRouter.h>

#include <g_RouterUtilsModuleInterface.h>
#include <g_SiphonModuleInterface.h>
#include <g_SystemTraceModuleStatic.h>

#include "command_line_parser.h"
#include "definitions.h"

int main(int argc, char* argv[])
{
    try
    {
#ifdef _WIN32
        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif
        CommandLineParser command_line_parser(argc, argv);

        command_line_parser.SetHelpOption("--help", kCommandLineHelpOptionText);

        Int16CommandLineParameter port_parameter("--port",
                                                 kCommandLinePortOptionText,
                                                 false,  // "Is this parameter required?"
                                                 kDefaultConnectionPort);
        command_line_parser.AddParameter(&port_parameter);

        if (!command_line_parser.Parse())
        {
            fprintf(stderr, "[RDS] Error parsing commandline arguments.\n");
            fprintf(stderr, "%s\n", command_line_parser.ErrorString().c_str());
            return 1;
        }

        if (command_line_parser.IsHelpRequested())
        {
            printf("Usage: %s %s", kRadeonDeveloperServiceCliFilename, kCommandLineUsageText);
            printf("%s", command_line_parser.HelpString().c_str());
            return -1;
        }

        // Create Router
        DDRouterCreateInfo info = {};
        info.pDescription       = kRadeonDeveloperServiceCliFilename;
        if (port_parameter.IsValid())
        {
            info.localPort = static_cast<uint16_t>(port_parameter.GetIntValue());
        }

        DDRouter  router = DD_API_INVALID_HANDLE;
        DD_RESULT result = ddRouterCreate(&info, &router);
        if (result != DD_RESULT_SUCCESS)
        {
            fprintf(stderr, "Failed to create router context: %s\n", ddRouterResultToString(result));
            return 1;
        }

        result = ddRouterLoadBuiltinModule(router, SystemTraceQueryModule(), nullptr);
        if (result != DD_RESULT_SUCCESS)
        {
            fprintf(stderr, "Failed to load router module: %s\n", ddRouterResultToString(result));
            ddRouterDestroy(router);
            router = DD_API_INVALID_HANDLE;
            return 1;
        }

        result = ddRouterLoadBuiltinModule(router, RouterUtilsQueryModuleInterface(), nullptr);
        if (result != DD_RESULT_SUCCESS)
        {
            fprintf(stderr, "Failed to load router utils module: %s\n", ddRouterResultToString(result));
            ddRouterDestroy(router);
            router = DD_API_INVALID_HANDLE;
            return 1;
        }

        result = ddRouterLoadBuiltinModule(router, SiphonQueryModuleInterface(), nullptr);
        if (result != DD_RESULT_SUCCESS)
        {
            fprintf(stderr, "Failed to load siphon module: %s\n", ddRouterResultToString(result));
            ddRouterDestroy(router);
            router = DD_API_INVALID_HANDLE;
            return 1;
        }

        printf("[RDS] Initialized successfully. Now listening for RDP connection.\n");

        // Main loop
        // Idle until the user exits. Ctrl-C will force kill the process too.
        printf("Type \"q\" or \"quit\" to exit\n");
        while (true)
        {
            std::string line;
            std::getline(std::cin, line);

            if (line == "q" || line == "quit")
            {
                break;
            }
        }

        // Destroy router
        if (router != DD_API_INVALID_HANDLE)
        {
            ddRouterDestroy(router);
        }
        return 0;
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Unexpected error: %s\n", e.what());
        return 1;
    }
    catch (...)
    {
        fprintf(stderr, "Unknown error occurred\n");
        return 1;
    }
}
