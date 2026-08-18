// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for capture delay widget.

#include "capture_delay_widget.h"

#include <QCheckBox>
#include <QSpinBox>

#include "ui_capture_delay_widget.h"

CaptureDelayWidget::CaptureDelayWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::CaptureDelayWidget)
{
    ui_->setupUi(this);

    connect(ui_->capture_delay_spin_box, QOverload<int>::of(&QSpinBox::valueChanged), this, &CaptureDelayWidget::CaptureDelayChanged);
    connect(ui_->enable_delay, &QCheckBox::stateChanged, this, &CaptureDelayWidget::ShouldDelayCaptureChanged);
}

CaptureDelayWidget::~CaptureDelayWidget() = default;

void CaptureDelayWidget::OnCaptureDelayChanged(uint32_t capture_delay)
{
    ui_->capture_delay_spin_box->setValue(capture_delay);
}

void CaptureDelayWidget::OnShouldDelayCaptureChanged(bool should_delay)
{
    ui_->enable_delay->setChecked(should_delay);
    ui_->delay_stack->setVisible(should_delay);
}
