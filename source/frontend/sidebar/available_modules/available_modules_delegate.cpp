// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the available modules widget delegate.

#include "available_modules_delegate.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QIcon>
#include <QPainter>
#include <QToolTip>

#include "collapsible_pane.h"
#include "models/module_model.h"

namespace rdp
{
    AvailableModulesItemDelegate::AvailableModulesItemDelegate(QObject* parent)
        : NoFocusDelegate(parent)
        , enable_button_icon_(":/circle-plus-enabled.svg")
        , enable_button_icon_disabled_(":/circle-plus-disabled.svg")
        , warning_icon_(":/warning.svg")
    {
    }

    void AvailableModulesItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        NoFocusDelegate::paint(painter, option, index);
        painter->save();

        const int   enable_button_size = CollapsiblePaneStatics::GetCollapsiblePaneButtonSize();
        const QRect enable_button_rect = GetEnabledButtonRect(option.rect, enable_button_size, option.fontMetrics);

        const QIcon& enable_icon = are_modules_locked_ ? enable_button_icon_disabled_ : enable_button_icon_;

        const bool   is_incompatible = index.siblingAtColumn(ModuleModel::ModuleModelColumns::kModuleModelColumnsIncompatible).data().toBool();
        const QIcon& icon            = is_incompatible ? warning_icon_ : enable_icon;
        icon.paint(painter, enable_button_rect);

        painter->restore();
    }

    QSize AvailableModulesItemDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        const int   button_size = CollapsiblePaneStatics::GetCollapsiblePaneButtonSize();
        const int   row_height  = qMax(option.fontMetrics.height(), button_size) + GetEnableButtonVerticalPadding(option.fontMetrics) * 2;
        const QSize super_size  = QStyledItemDelegate::sizeHint(option, index);
        return {super_size.width(), row_height};
    }

    bool AvailableModulesItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        Q_UNUSED(model)

        if (event->type() != QEvent::MouseButtonPress || are_modules_locked_)
        {
            // Consume everything, but only handle mouse events
            return true;
        }

        QMouseEvent* mouse_event        = static_cast<QMouseEvent*>(event);
        const int    button_size        = CollapsiblePaneStatics::GetCollapsiblePaneButtonSize();
        const QRect  enable_button_rect = GetEnabledButtonRect(option.rect, button_size, option.fontMetrics);

        if (enable_button_rect.contains(mouse_event->pos()))
        {
            emit PressedEnable(index);
        }

        return true;
    }

    bool AvailableModulesItemDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        if (event->type() != QEvent::ToolTip)
        {
            return QStyledItemDelegate::helpEvent(event, view, option, index);
        }

        const QString tooltip = index.data(Qt::ToolTipRole).toString();
        if (tooltip.isEmpty())
        {
            return false;
        }

        QHelpEvent* help_event         = static_cast<QHelpEvent*>(event);
        const QRect enable_button_rect = GetEnabledButtonRect(option.rect, CollapsiblePaneStatics::GetCollapsiblePaneButtonSize(), option.fontMetrics);

        if (enable_button_rect.contains(help_event->pos()))
        {
            QToolTip::showText(view->mapToGlobal(help_event->pos()), tooltip, view);
            return true;
        }

        return false;
    }

    QRect AvailableModulesItemDelegate::GetEnabledButtonRect(const QRect& item_rect, int button_size, const QFontMetrics& font_metrics)
    {
        return {item_rect.width() - button_size, item_rect.top() + GetEnableButtonVerticalPadding(font_metrics), button_size, button_size};
    }

    int AvailableModulesItemDelegate::GetEnableButtonVerticalPadding(const QFontMetrics& font_metrics)
    {
        return font_metrics.height() / 3;
    }

    void AvailableModulesItemDelegate::SetModulesLocked(bool are_modules_locked)
    {
        are_modules_locked_ = are_modules_locked;
    }
}  // namespace rdp
