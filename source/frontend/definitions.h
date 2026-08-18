// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Header file for the RDP definitions

#ifndef RDP_SOURCE_FRONTEND_DEFINITIONS_H_
#define RDP_SOURCE_FRONTEND_DEFINITIONS_H_

#define MODULE_DATA_CHANGE_EVENT (QEvent::User + 1)

static constexpr auto kRadeonDeveloperPanelGuid      = "C9EB0587-F8F7-4B8C-B35A-F7C2862CFDA7";  ///< Unique identifier for the application
static constexpr auto kAllWorkflowName               = "All";
static constexpr auto kDefaultWorkflowName           = "Default";  // For legacy support
static constexpr auto kAmdVulkan                     = "AMD Vulkan Driver";
static constexpr auto kAmdDirectX12                  = "AMD DirectX12 Driver";
static constexpr auto kAmdOpenCl                     = "AMD OpenCL Driver";
static constexpr auto kAmdHip                        = "AMD HIP Driver";
static constexpr auto kAmdOpenGl                     = "AMD OpenGL Driver";
static constexpr auto kAmdDirectX9                   = "AMD DirectX9 Driver";
static constexpr auto kAmdDirectX11                  = "AMD DirectX10/11 Driver";
static constexpr auto kWindowsIdentifier             = "Windows";
static constexpr auto kLinuxIdentifier               = "Linux";
static constexpr int  kWorkflowNameMaxLength         = 32;
static constexpr auto kPeakClocksWritableDialogTitle = "Peak clock mode disabled";
static constexpr auto kPeakClocksWritableDialogMessage =
    "Peak clock mode may be disabled. Please run the 'setup.sh' "
    "script in the scripts folder to use the RGP profiling feature. This should be "
    "done after every system reboot.";
static constexpr auto kEtwWarningTitle = "Event Tracing for Windows disabled";
static constexpr auto kEtwWarningText =
    "Unable to open Event Tracing for Windows\nThe following features may not function: Resource naming for memory tracing and Signals/Waits capture for "
    "DX12 "
    "profiling.%2\nPlease run the AddUserToGroup.bat file as administrator to enable ETW. See documentation for more "
    "information.";
static constexpr auto kEtwNoPermissionText         = "\n\nPlease be sure to run the AddUserToGroup.bat script in order to apply account privileges.";
static constexpr auto kApplicationSettingsThemeKey = "application_theme";

#endif
