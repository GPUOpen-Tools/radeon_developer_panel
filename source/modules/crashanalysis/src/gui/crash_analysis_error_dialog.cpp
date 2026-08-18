// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Class analysis error dialog implementation.

#include "crash_analysis_error_dialog.h"

#include "ui_crash_analysis_error_dialog.h"

CrashAnalysisErrorDialog::CrashAnalysisErrorDialog()
    : ui_(new Ui::CrashAnalysisErrorDialog)
{
    ui_->setupUi(this);

    // Remove the hint button
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    connect(ui_->ok_button, &QPushButton::pressed, this, &CrashAnalysisErrorDialog::OnPressedOkButton);
}

CrashAnalysisErrorDialog::~CrashAnalysisErrorDialog() = default;

void CrashAnalysisErrorDialog::SetErrorMessage(const QString& error) const
{
    ui_->error_contents->setText(error);
}

void CrashAnalysisErrorDialog::OnPressedOkButton()
{
    close();
}
