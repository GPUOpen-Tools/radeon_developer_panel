// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A widget that shows the status of a trace being captured.

#include "capture_progress_widget.h"

#include <qt_common/utils/qt_util.h>

#include "common/inc/definitions.h"

#include "ui_capture_progress_widget.h"

CaptureProgressWidget::CaptureProgressWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::CaptureProgressWidget)
{
    // Initialize the UI and set white background color
    ui_->setupUi(this);

    // Update to use the Busy cursor when a trace is being collected.
    setCursor(Qt::BusyCursor);

    // Connect the cancel button to the OnClicked handler.
    connect(ui_->cancelTraceButton, &QPushButton::clicked, this, &CaptureProgressWidget::OnCancelTraceClicked);
}

CaptureProgressWidget::~CaptureProgressWidget()
{
    // Revert to a normal Arrow cursor when the trace is finished.
    setCursor(Qt::ArrowCursor);
}

void CaptureProgressWidget::Reset()
{
    ui_->progressBar->setValue(0);
}

void CaptureProgressWidget::SetCancelButtonShown(bool shown)
{
    ui_->cancelTraceButton->setVisible(shown);
}

void CaptureProgressWidget::UpdateProgress(ProgressInfo progress_info)
{
    SetProgressText(progress_info.progress_text);

    if (progress_info.progress == 0.0)
    {
        ui_->progressBar->setMinimum(0);
        ui_->progressBar->setMaximum(0);

        return;
    }

    ui_->progressBar->setMinimum(0);
    ui_->progressBar->setMaximum(100);

    const int progress_value = static_cast<int>(progress_info.progress * ui_->progressBar->maximum());
    ui_->progressBar->setValue(progress_value);
}

void CaptureProgressWidget::SetProgressText(const QString& text)
{
    ui_->transferProgressLabel->setText(text);
}

void CaptureProgressWidget::OnCancelTraceClicked()
{
    emit TraceCancelled();
}
