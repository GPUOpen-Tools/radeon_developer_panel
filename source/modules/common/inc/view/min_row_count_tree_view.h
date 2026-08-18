// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for tree view that will have a minimum size that displays a minimum amount of rows.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_VIEW_MIN_ROW_COUNT_TREE_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_VIEW_MIN_ROW_COUNT_TREE_VIEW_H_

#include <QTreeView>

/// @brief A scaled tree view whose minimum size is calculated such that it fits a fixed number of rows.
class MinRowCountTreeView : public QTreeView
{
public:
    /// @brief Constructor.
    /// @param [in] parent The parent widget.
    explicit MinRowCountTreeView(QWidget* parent = nullptr);

    /// @brief Returns the minimum size hint for this view such that it displays a fixed number of rows.
    /// @return The minimum size hint for this widget.
    QSize minimumSizeHint() const override;
};

#endif
