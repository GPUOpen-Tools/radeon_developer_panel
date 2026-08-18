// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for a line edit that emits signals when it is focused / unfocused.

#include "common/inc/focus_notification_line_edit.h"

FocusNotificationLineEdit::FocusNotificationLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
}

void FocusNotificationLineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    emit FocusIn();
}

void FocusNotificationLineEdit::focusOutEvent(QFocusEvent* event)
{
    QLineEdit::focusOutEvent(event);
    emit FocusOut();
}

void FocusNotificationLineEdit::SetMinimumText(const QString& minimum_text)
{
    minimum_text_ = minimum_text;
    updateGeometry();
}

QSize FocusNotificationLineEdit::minimumSizeHint() const
{
    const int text_width = fontMetrics().horizontalAdvance(minimum_text_);
    const int margins    = textMargins().left() + textMargins().right();

    return {text_width + margins, QLineEdit::minimumSizeHint().height()};
}
