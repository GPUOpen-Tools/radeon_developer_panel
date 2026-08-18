// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for spin box that doesn't scroll with the scroll wheel

#include "rdp_spin_box.h"

#include <QWheelEvent>

RdpSpinBox::RdpSpinBox(QWidget* parent)
    : QSpinBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void RdpSpinBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();
    }
    else
    {
        QSpinBox::wheelEvent(event);
    }
}
