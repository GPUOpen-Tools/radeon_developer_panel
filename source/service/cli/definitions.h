// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Shared definitions for the Radeon Developer Service.

#ifndef RDP_SOURCE_SERVICE_CLI_DEFINITIONS_H_
#define RDP_SOURCE_SERVICE_CLI_DEFINITIONS_H_

#include <cstdint>

inline constexpr auto kRadeonDeveloperServiceCliFilename = "RadeonDeveloperServiceCLI";  ///< RDS CLI executable filename.
inline constexpr auto kCommandLineUsageText              = " [--help] | [--port <port-number>]\n\n";
inline constexpr auto kCommandLineHelpOptionText         = "--help               This help message.";  ///< Command line help for --help
inline constexpr auto kCommandLinePortOptionText =
    "--port <port-number>  The listener port.  Where <port-number> is a value between 1 and 65535.";  ///< Command line help for --port
inline constexpr unsigned int kDefaultConnectionPort = 27300;  ///< The default port used to connect the Developer Panel to the Developer Service.
inline constexpr uint16_t     kMaxListenPort         = 65535;  ///< The highest port that the Developer Service can listen on.
inline constexpr auto         kProductNameString     = "Radeon Developer Service - CLI";  ///< The human-readable product name.

#endif
