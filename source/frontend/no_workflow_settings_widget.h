// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Placeholder view for Workflow's with no user settings

#ifndef RDP_SOURCE_FRONTEND_NO_WORKFLOW_SETTINGS_WIDGET_H_
#define RDP_SOURCE_FRONTEND_NO_WORKFLOW_SETTINGS_WIDGET_H_

#include <memory>

#include <QWidget>

namespace Ui
{
    class NoWorkflowSettingsWidget;
}

namespace rdp
{
    /// @brief Placeholder view for Workflow user settings
    class NoWorkflowSettingsWidget : public QWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] parent The parent widget
        explicit NoWorkflowSettingsWidget(QWidget* parent = nullptr);

        /// @brief Destructor
        ~NoWorkflowSettingsWidget() override;

    private:
        std::unique_ptr<Ui::NoWorkflowSettingsWidget> ui_;  ///< Qt ui
    };
}  // namespace rdp

#endif
