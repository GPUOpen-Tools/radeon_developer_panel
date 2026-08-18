// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for item delegate that will show selection even if there is no focus.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_SHOW_SELECTION_NO_FOCUS_H_
#define RDP_SOURCE_MODULES_COMMON_INC_SHOW_SELECTION_NO_FOCUS_H_

#include <QStyledItemDelegate>

/// @brief A delegate that will always show an item as selected even if there is not focus.
class ShowSelectionNoFocusDelegate : public QStyledItemDelegate
{
public:
    /// @brief Constructor.
    /// @param [in] parent The parent object.
    explicit ShowSelectionNoFocusDelegate(QObject* parent);

    void                paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

#endif
