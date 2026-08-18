// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for class that produces bug reports.

#ifndef RDP_SOURCE_FRONTEND_BUG_BUG_REPORT_GENERATOR_H_
#define RDP_SOURCE_FRONTEND_BUG_BUG_REPORT_GENERATOR_H_

#include <memory>

#include <QObject>

#include "../models/system_info_model.h"
#include "bug_report.h"

namespace rdp
{
    /// @brief Class responsible for generating bug reports that include system information.
    class BugReportGenerator : public QObject
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] system_info_model The model that has the system info.
        /// @param [in] Model that contains information about the modules.
        BugReportGenerator(const std::shared_ptr<class SystemInfoModel>& system_info_model, const std::shared_ptr<class ModuleModel>& module_model);

    public slots:  // NOLINT(readability-redundant-access-specifiers)

        /// @brief Copies a bug report to the clipboard.
        void CopyReport();

    private:
        /// @brief Fills a bug report with all the available information.
        /// @param report_details The report to fill with information.
        void FillBugReport(BugReport& report_details) const;

        /// @brief Generates the text for a bug report.
        /// @param report_details The details of the report to fill into the template.
        /// @return The complete text of a bug report.
        QString GenerateReport(BugReport& report_details) const;

        /// @brief Generates the text for a all the GPUs in a report using the GPU template.
        /// @param report_details The details of the report to fill into the template.
        /// @return The complete GPU text.
        QString GenerateGpuInfo(BugReport& report_details) const;

        /// @brief Replaces a field in the template with the string representation of the value.
        /// @param report The report so far, where to replace the field in.
        /// @param name The name of the field to replace.
        /// @param value The value of the field.
        /// @param value_present true if the value was actually present, false otherwise.
        static void ReplaceField(QString& report, const char* name, const QVariant& value, bool value_present = true);

        /// @brief Replaces a field in the template with the hex string representation of the value.
        /// @param report The report so far, where to replace the field in.
        /// @param name The name of the field to replace.
        /// @param value The value of the field.
        static void ReplaceFieldHex(QString& report, const char* name, uint64_t value);

        QString template_;            ///< The template for a bug report.
        QString gpu_template_;        ///< The template for the information about a single GPU.
        int     gpu_template_start_;  ///< The starting position in the main string for the GPU template.

        std::shared_ptr<SystemInfoModel>   system_info_model_;  ///< Model that contains information about the system that RDP is connected to.
        std::shared_ptr<class ModuleModel> module_model_;       ///< Model that contains information about the modules.
    };

}  // namespace rdp

#endif
