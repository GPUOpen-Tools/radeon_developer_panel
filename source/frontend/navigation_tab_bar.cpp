// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for NavigationTabBar class.

#include "navigation_tab_bar.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSizePolicy>
#include <QTabBar>
#include <QWidget>

namespace rdp
{

    NavigationTabBar::NavigationTabBar(QWidget* parent)
        : QTabBar(parent)
        , stretch_last_(false)
        , spacer_index_(-1)
        , mouse_hover_last_tab_index_(-1)
    {
        setMouseTracking(true);
    }

    void NavigationTabBar::mouseMoveEvent(QMouseEvent* event)
    {
        int tab_index = QTabBar::tabAt(event->pos());

        // Only change mouse cursor if the mouse is hovoring over a different tab.
        if (mouse_hover_last_tab_index_ != tab_index)
        {
            if (isTabEnabled(tab_index))
            {
                setCursor(Qt::PointingHandCursor);
            }
            else
            {
                setCursor(Qt::ArrowCursor);
            }
            mouse_hover_last_tab_index_ = tab_index;
        }
    }

    void NavigationTabBar::setTabEnabled(int index, bool enable)
    {
        // Force next mouseMoveEvent to set mouse cursor even if mouse is hovoring over
        // the same tab as the last event call.
        mouse_hover_last_tab_index_ = -1;

        QTabBar::setTabEnabled(index, enable);
    }

    QSize NavigationTabBar::minimumTabSizeHint(int index) const
    {
        if (index == SpacerIndex())
        {
            return QSize(0, QTabBar::tabSizeHint(index).height());
        }
        else
        {
            return QTabBar::minimumTabSizeHint(index);
        }
    }

    QSize NavigationTabBar::tabSizeHint(int index) const
    {
        // Make sure to polish the tab bar. This makes
        // sure it has the actual font from the stylesheet
        this->ensurePolished();

        int height = QTabBar::tabSizeHint(index).height();
        if (index == SpacerIndex())
        {
            return QSize(CalcSpacerWidth(), height);
        }
        else if (tabText(index).isEmpty())
        {
            int      width  = 0;
            QWidget* widget = tabButton(index, QTabBar::ButtonPosition::LeftSide);
            if (widget)
            {
                width = widget->sizeHint().width();
            }

            widget = tabButton(index, QTabBar::ButtonPosition::RightSide);
            if (widget)
            {
                width = widget->sizeHint().width();
            }
            return QSize(width, height);
        }
        else
        {
            QSize size_hint = QTabBar::tabSizeHint(index);
            return size_hint;
        }
    }

    void NavigationTabBar::SetSpacerIndex(int index)
    {
        if (index != -1)
        {
            setTabEnabled(index, false);
            setTabText(index, "");
            adjustSize();
        }
        spacer_index_ = index;
    }

    void NavigationTabBar::SetStretchLast(bool stretch)
    {
        stretch_last_ = stretch;
    }

    void NavigationTabBar::SetTabTool(int index, QWidget* button_widget, QTabBar::ButtonPosition pos)
    {
        setTabText(index, "");
        setTabEnabled(index, false);
        setTabButton(index, pos, button_widget);
    }

    int NavigationTabBar::SpacerIndex() const
    {
        return spacer_index_;
    }

    int NavigationTabBar::CalcSpacerWidth() const
    {
        if ((count() == 0) || (spacer_index_ < 0))
        {
            return 0;
        }

        int spacer_width = parentWidget()->width();
        for (int i = 0; i < count(); i++)
        {
            if (i != spacer_index_)
            {
                spacer_width -= tabRect(i).width();
            }
        }

        return spacer_width;
    }
}  // namespace rdp
