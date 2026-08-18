// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for class that produces bug reports.

#include <stdexcept>

#include <qconfig.h>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QResource>
#include <utility>

#include <ddRouter.h>

#include <qt_common/custom_widgets/message_overlay.h>

#include <common/inc/api/dev_tools_module.h>

#include "bug_report_generator.h"
#include "models/module_model.h"
#include "utilities.h"
#include "version.h"

namespace rdp
{
    /// @brief The path to the template file.
    static constexpr char const* kBugReportTemplatePath = ":bug_report_template.txt";

    /// @brief The text to fill if a property should be supplied by the user.
    static constexpr char const* kFillMeIn = "(fill me in)";

    /// @brief The text to fill if a property doesn't have a value.
    static constexpr char const* kNoneValue = "N/A";

    /// @brief The marker in the template for where the GPU template starts.
    static constexpr char const* kGpuTemplateStartMarker = "<start_gpu>";

    /// @brief The marker in the template for where the GPU template ends.
    static constexpr char const* kGpuTemplateEndMarker = "<end_gpu>";

    /// @brief The text displayed in a message box to indicate that a bug report has been copied.
    static constexpr char const* kReportCopiedMessage = "A bug report template with system information has been copied to the clipboard.";

    BugReportGenerator::BugReportGenerator(const std::shared_ptr<class SystemInfoModel>& system_info_model,
                                           const std::shared_ptr<class ModuleModel>&     module_model)
        : system_info_model_(system_info_model)
        , module_model_(module_model)
    {
        QResource resource(kBugReportTemplatePath);
        template_ = QString(reinterpret_cast<const char*>(resource.data()));

        gpu_template_start_  = template_.indexOf(kGpuTemplateStartMarker);
        int gpu_template_end = template_.indexOf(kGpuTemplateEndMarker);

        if (gpu_template_start_ == -1 || gpu_template_end == -1)
        {
            throw std::runtime_error("Could not find GPU template start / end marker in the bug report template");
        }

        int gpu_template_with_start_length = gpu_template_end - gpu_template_start_;
        gpu_template_                      = template_.mid(gpu_template_start_, gpu_template_with_start_length).replace(kGpuTemplateStartMarker, "");

        // Remove the GPU template from the main template
        template_ = template_.remove(gpu_template_start_, gpu_template_with_start_length + static_cast<int>(strlen(kGpuTemplateEndMarker)));
    }

    void BugReportGenerator::CopyReport()
    {
        BugReport report_details;
        FillBugReport(report_details);

        QString report = GenerateReport(report_details);
        QApplication::clipboard()->setText(report);

        MessageOverlay::InfoAsync("Bug Report", kReportCopiedMessage);
    }

    void BugReportGenerator::FillBugReport(BugReport& report) const
    {
        if (system_info_model_ != nullptr)
        {
            system_info_model_->FillBugReport(report);
        }

        report.rdp_version       = RDP_TITLE;
        report.build_date        = RDP_BUILD_DATE_STRING;
        report.host_os_name      = util::PrettyOsName();
        report.qt_version        = QT_VERSION_STR;
        report.dd_router_version = ddRouterQueryVersionString();

        for (const DevToolsModule* module : module_model_->GetEnabledModules())
        {
            report.enabled_modules_.push_back(module->GetModuleDisplayName().c_str());
        }
    }

    QString BugReportGenerator::GenerateReport(BugReport& report_details) const
    {
        QString report = template_;

        // Generate the GPU info first so that the insertion position is correct
        const QString gpu_info = GenerateGpuInfo(report_details);
        report.insert(gpu_template_start_, gpu_info);

        // Host properties
        ReplaceField(report, "rdp_version", report_details.rdp_version);
        ReplaceField(report, "build_date", report_details.build_date);

        ReplaceField(report, "host_os_name", report_details.host_os_name);
        ReplaceField(report, "qt_version", report_details.qt_version);

        ReplaceField(report, "dd_tool_version", report_details.dd_tool_version);
        ReplaceField(report, "dd_router_version", report_details.dd_router_version);

        if (!report_details.enabled_modules_.isEmpty())
        {
            ReplaceField(report, "enabled_modules", report_details.enabled_modules_.join(", "));
        }
        else
        {
            ReplaceField(report, "enabled_modules", "None");
        }

        // Connection properties
        auto replace_if_connected = [&](const char* name, const QVariant& value) { ReplaceField(report, name, value, report_details.was_connected); };

        replace_if_connected("driver_packaging_version", report_details.driver_packaging_version);
        replace_if_connected("driver_build", kFillMeIn);
        replace_if_connected("operating_system_name", report_details.operating_system_name);
        replace_if_connected("operating_system_description", report_details.operating_system_description);
        replace_if_connected("gpu_open_interface_major_version", report_details.gpu_open_interface_major_version);

        return report;
    }

    QString BugReportGenerator::GenerateGpuInfo(BugReport& report_details) const
    {
        if (!report_details.was_connected || report_details.gpus.empty())
        {
            return kNoneValue;
        }

        QStringList gpus;

        for (const auto& gpu : report_details.gpus)
        {
            QString gpu_info = gpu_template_;

            ReplaceField(gpu_info, "name", gpu.name);
            ReplaceFieldHex(gpu_info, "device_id", gpu.device_id);
            ReplaceFieldHex(gpu_info, "revision", gpu.revision);
            ReplaceFieldHex(gpu_info, "family", gpu.family);
            ReplaceField(gpu_info, "gfx_engine", gpu.gfx_engine);

            gpus.push_back(gpu_info);
        }

        return gpus.join("\n\n");
    }

    void BugReportGenerator::ReplaceField(QString& report, const char* name, const QVariant& value, bool value_present)
    {
        const QString template_name     = QString("<%1>").arg(name);
        const QString replacement_value = value_present ? value.toString() : kNoneValue;
        report                          = report.replace(template_name, replacement_value);
    }

    void BugReportGenerator::ReplaceFieldHex(QString& report, const char* name, uint64_t value)
    {
        ReplaceField(report, name, QString::number(value, 16).toUpper());
    }

}  // namespace rdp
