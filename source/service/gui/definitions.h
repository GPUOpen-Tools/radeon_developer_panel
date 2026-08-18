// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Shared definitions for the Radeon Developer Service.

#ifndef RDP_SOURCE_SERVICE_GUI_DEFINITIONS_H_
#define RDP_SOURCE_SERVICE_GUI_DEFINITIONS_H_

static constexpr auto         kRadeonDeveloperServiceFilename = "RadeonDeveloperService";                ///< RDS's executable filename.
static constexpr auto         kRadeonDeveloperServiceGuid     = "D0939873-BA4B-4C4E-9729-D82DED85BC41";  ///< Unique identifier for the RDS service
static constexpr auto         kProductNameString              = "Radeon Developer Service";              ///< The human-readable product name.
static constexpr auto         kProductSettingsFilename        = "settings.ini";    ///< The filename for RDS application settings ini file.
static constexpr auto         kConfigureContextMenu           = "Configure";       ///< Configure action in the system tray menu.
static constexpr auto         kQuitContextMenu                = "Quit";            ///< Toggle displayed in system tray menu.
static constexpr auto         kRDSIconName                    = ":/RDS_Icon.ico";  ///< RDS Window Icon.
static constexpr uint16_t     kMaxListenPort                  = 65535;             ///< The highest port that the Developer Service can listen on.
static constexpr unsigned int kDefaultConnectionPort          = 27300;  ///< The default port used to connect the Developer Panel to the Developer Service.

#endif
