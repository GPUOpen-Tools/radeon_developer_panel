// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration and implementation for an item delegate thate removes the focus state.

#ifndef RDP_MODULES_COMMON_NO_FOCUS_DELEGATE_H_
#define RDP_MODULES_COMMON_NO_FOCUS_DELEGATE_H_

#include <QStyledItemDelegate>

/// @brief Style delegate that removes the focus border from an item.
class NoFocusDelegate : public QStyledItemDelegate
{
public:
    /// @brief Constructor.
    /// @param [in] parent The parent object.
    explicit NoFocusDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    /// @brief Paints the item, removing the focus flag if it has it.
    /// @param [in] painter The painter to paint the item with.
    /// @param [in] option The options for painting the item.
    /// @param [in] index The index of the item to paint.
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem item_option = QStyleOptionViewItem(option);
        if ((item_option.state & QStyle::State_HasFocus) > 0)
        {
            item_option.state = item_option.state ^ QStyle::State_HasFocus;
        }

        QStyledItemDelegate::paint(painter, item_option, index);
    }
};

#endif
