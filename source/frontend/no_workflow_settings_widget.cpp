// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Placeholder view for Workflow's with no user settings

#include "no_workflow_settings_widget.h"
#include "ui_no_workflow_settings_widget.h"

namespace rdp
{
    NoWorkflowSettingsWidget::NoWorkflowSettingsWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::NoWorkflowSettingsWidget)
    {
        ui_->setupUi(this);
    }

    NoWorkflowSettingsWidget::~NoWorkflowSettingsWidget()
    {
    }
}  // namespace rdp
