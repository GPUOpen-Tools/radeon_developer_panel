// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for collapsible pane button.

#include "collapsible_pane_button.h"
#include "collapsible_pane.h"

#include <QPaintEvent>
#include <QPainter>

CollapsiblePaneButton::CollapsiblePaneButton(QWidget* parent)
    : QPushButton(parent)
{
    setStyleSheet("QPushButton{ border: none;}");
}

QSize CollapsiblePaneButton::sizeHint() const
{
    int button_size = CollapsiblePaneStatics::GetCollapsiblePaneButtonSize();
    return {button_size, button_size};
}

void CollapsiblePaneButton::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    setIconSize(event->size());
}
