// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for tree view that will have a minimum size that displays a minimum amount of rows.

#include "common/inc/view/min_row_count_tree_view.h"

#include <QHeaderView>

static constexpr int kMinNumberRows = 14;

MinRowCountTreeView::MinRowCountTreeView(QWidget* parent)
    : QTreeView(parent)
{
}

QSize MinRowCountTreeView::minimumSizeHint() const
{
    const QAbstractItemDelegate* item_delegate = itemDelegate();

    QStyleOptionViewItem item;
    initViewItemOption(&item);
    const QSize size_hint       = item_delegate->sizeHint(item, {});
    const int   row_height      = size_hint.height();
    const QSize super_size_hint = QTreeView::minimumSizeHint();

    // We increment one here to account for the header
    return {super_size_hint.width(), row_height * kMinNumberRows + header()->height()};
}
