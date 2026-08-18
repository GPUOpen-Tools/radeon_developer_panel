// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for item delegate that will show selection even if there is no focus.

#include "show_selection_no_focus_delegate.h"

ShowSelectionNoFocusDelegate::ShowSelectionNoFocusDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ShowSelectionNoFocusDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QStyleOptionViewItem modified_option = option;

    // This ensures that the style of the selected items is always the same regardless if the window is in focus or not.
    if ((option.state & QStyle::State_Selected) > 0)
    {
        modified_option.state = (option.state | QStyle::State_Active) & (~QStyle::State_HasFocus);
    }

    // Resize the decoration to fit perfectly in the cell since the decoration size will have been calculated from the QIcon.
    const int decoration_size      = option.rect.height();
    modified_option.decorationSize = QSize(decoration_size, decoration_size);

    QStyledItemDelegate::paint(painter, modified_option, index);
}

QSize ShowSelectionNoFocusDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // By zeroing the decoration size, we ensure that it doesn't contribute to the height of the row
    QStyleOptionViewItem modified_option = option;
    modified_option.decorationSize       = {};

    return QStyledItemDelegate::sizeHint(modified_option, index);
}
