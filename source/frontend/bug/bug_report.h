// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for all the information needed for a bug report.

#ifndef RDP_SOURCE_FRONTEND_BUG_BUG_REPORT_H_
#define RDP_SOURCE_FRONTEND_BUG_BUG_REPORT_H_

#include <vector>

#include <QString>

namespace rdp
{
    /// @brief Information about a specific GPU for a bug report.
    struct BugReportGpu
    {
        QString  name;        ///< The name of the graphics card.
        uint32_t device_id;   ///< The asic device identifier.
        uint32_t revision;    ///< The asic revision.
        uint32_t family;      ///< The asic family.
        uint32_t gfx_engine;  ///< The gfx engine of the asic
    };

    // @brief The information needed to generate a bug report template.
    struct BugReport
    {
        QString rdp_version;  ///< The version of RDP that is running.
        QString build_date;   ///< The date that the version of RDP was built on.

        QString host_os_name;  ///< The name of the operating system that RDP is running on.
        QString qt_version;    ///< The Qt version string.

        QString dd_tool_version;    ///< The version of dev driver (link-time).
        QString dd_router_version;  ///< The version of the dev driver router (link-time).

        QStringList enabled_modules_;  ///< The enabled modules.

        // These are from the system that the system that RDP is connected to. If a bug is reported while
        // RDP is disconnected from RDS, all of these value should be empty
        QString                   driver_packaging_version;          ///< The packaging version of the driver on the connected system.
        QString                   operating_system_name;             ///< The name of the operating system of the connected system.
        QString                   operating_system_description;      ///< The description of the operating system of the connected system.
        uint32_t                  gpu_open_interface_major_version;  ///< The GPU open interface version.
        std::vector<BugReportGpu> gpus;                              ///< The graphics cards in the connected system.
        bool                      was_connected = false;             ///< true if a system was connected when the report was generated.
    };

}  // namespace rdp

#endif
