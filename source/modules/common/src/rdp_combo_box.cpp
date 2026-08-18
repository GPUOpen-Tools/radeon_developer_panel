// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for combo box that doesn't scroll with the scroll wheel

#include "rdp_combo_box.h"

#include <QWheelEvent>

RdpComboBox::RdpComboBox(QWidget* parent)
    : QComboBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void RdpComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus())
    {
        event->ignore();
    }
    else
    {
        QComboBox::wheelEvent(event);
    }
}
