// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for NavigationTabWidget class.

#include "navigation_tab_widget.h"

#include <QResizeEvent>

#include "navigation_tab_bar.h"

namespace rdp
{

    NavigationTabWidget::NavigationTabWidget(QWidget* parent)
        : QTabWidget(parent)
    {
        tab_bar_ = new NavigationTabBar(this);

        // Replace the TabWidget's QTabBar with a custom one.
        setTabBar(tab_bar_);
    }

    QTabBar* NavigationTabWidget::tabBar() const
    {
        return QTabWidget::tabBar();
    }

    int NavigationTabWidget::TabHeight() const
    {
        return tabBar()->height();
    }

    void NavigationTabWidget::resizeEvent(QResizeEvent* resize_event)
    {
        tab_bar_->resize(resize_event->size());
        QTabWidget::resizeEvent(resize_event);
    }

    void NavigationTabWidget::SetTabEnabled(int index, bool enable)
    {
        tab_bar_->setTabEnabled(index, enable);
    }

    void NavigationTabWidget::SetSpacerIndex(const int index)
    {
        tab_bar_->SetSpacerIndex(index);
    }

    void NavigationTabWidget::SetStretchLast(const bool stretch)
    {
        tab_bar_->SetStretchLast(stretch);
    }

    void NavigationTabWidget::SetTabTool(int index, QWidget* tool_widget, QTabBar::ButtonPosition pos)
    {
        tab_bar_->SetTabTool(index, tool_widget, pos);
    }

}  // namespace rdp
