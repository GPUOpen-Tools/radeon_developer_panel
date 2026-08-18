// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the tree view that is used for the debug log widget.

#include "debug_log_tree_view.h"

#include <QHeaderView>

static constexpr int kMinimumSectionSize = 80;

namespace rdp
{
    DebugLogTreeView::DebugLogTreeView(QWidget* parent)
        : QTreeView(parent)
    {
    }

    void DebugLogTreeView::setModel(QAbstractItemModel* model)
    {
        QTreeView::setModel(model);

        header()->setMinimumSectionSize(kMinimumSectionSize);
        header()->setSectionResizeMode(QHeaderView::ResizeToContents);
        header()->setStretchLastSection(false);

        setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
        UpdateColumnWidths();
    }

    QSize DebugLogTreeView::sizeHint() const
    {
        return {};
    }

    QSize DebugLogTreeView::minimumSizeHint() const
    {
        return {};
    }

    void DebugLogTreeView::resizeEvent(QResizeEvent* event)
    {
        UpdateColumnWidths();
        QAbstractItemView::resizeEvent(event);
    }

    void DebugLogTreeView::UpdateColumnWidths()
    {
        if (header()->count() == 0)
        {
            return;
        }

        const int last_column        = header()->count() - 1;
        int       left_columns_width = 0;

        for (int column = 0; column < last_column; ++column)
        {
            left_columns_width += columnWidth(column);
        }

        if (sizeHintForColumn(last_column) + left_columns_width < viewport()->width())
        {
            header()->setSectionResizeMode(last_column, QHeaderView::Fixed);
            header()->setDefaultSectionSize(viewport()->width() - left_columns_width);

            return;
        }

        header()->setSectionResizeMode(last_column, QHeaderView::ResizeToContents);
    }
};  // namespace rdp
