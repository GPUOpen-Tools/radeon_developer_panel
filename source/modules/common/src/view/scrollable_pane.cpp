// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a scrollable pane that contains collapsible widgets.

#include "view/scrollable_pane.h"

#include <QLayout>
#include <QScrollBar>

static constexpr int kSidebarHorizontalMarginPadding = 3;
static constexpr int kSidebarVerticalMargin          = 20;

void ScrollablePaneContents::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    emit Resized();
}

ScrollablePane::ScrollablePane(QWidget* parent)
    : QScrollArea(parent)
{
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Because the scroll bar overlays the padding, horizontal scrolling is actually possible.
    // This disables horizontal scrolling completely.
    horizontalScrollBar()->setEnabled(false);
}

QSize ScrollablePane::sizeHint() const
{
    // The desired for this widget is the max size needed for one of the children width-wise plus some margin to accommodate the scroll bar.
    // This ensures no horizontal scrolling but also that there is no movement of any child views from the scroll bar appearing / disappearing.
    const QSize    super_size_hint = QScrollArea::sizeHint();
    const QWidget* contents        = widget();
    if (contents == nullptr || contents->layout() == nullptr)
    {
        return super_size_hint;
    }

    int max_width = 0;
    for (int i = 0; i < contents->layout()->count(); ++i)
    {
        max_width = std::max(max_width, contents->layout()->itemAt(i)->sizeHint().width());
    }

    const int scroll_bar_width = verticalScrollBar()->sizeHint().width() + kSidebarHorizontalMarginPadding;
    return {max_width + scroll_bar_width * 2, super_size_hint.height()};
}

QSize ScrollablePane::minimumSizeHint() const
{
    const QSize super_size_hint = QScrollArea::minimumSizeHint();

    const QWidget* contents = widget();
    if (contents == nullptr || contents->layout() == nullptr)
    {
        return super_size_hint;
    }

    return {contents->minimumSizeHint().width(), super_size_hint.height()};
}

void ScrollablePane::resizeEvent(QResizeEvent* event)
{
    const QWidget* contents = widget();
    if (contents == nullptr || contents->layout() == nullptr)
    {
        QScrollArea::resizeEvent(event);
        return;
    }

    const int scroll_bar_width = verticalScrollBar()->sizeHint().width() + kSidebarHorizontalMarginPadding;

    // The margins are calculated such that there is enough space for the scroll bar to be overlaid on the margins (plus some padding)
    // For some reason, when the width of the widget is greater than the size hint, the scroll bar is no longer overlaid on the margins.
    // In this case, we shrink the margin to account for this behavior.
    const int right_margin = verticalScrollBar()->isVisible() && geometry().width() > sizeHint().width() ? kSidebarHorizontalMarginPadding : scroll_bar_width;

    contents->layout()->setContentsMargins(scroll_bar_width, kSidebarVerticalMargin, right_margin, kSidebarVerticalMargin);
    updateGeometry();

    // The super call is made at the end here to avoid some visual artifacts.
    QScrollArea::resizeEvent(event);
}

void ScrollablePane::showEvent(QShowEvent* event)
{
    const ScrollablePaneContents* contents = qobject_cast<ScrollablePaneContents*>(widget());
    Q_ASSERT(contents != nullptr);

    if (contents != nullptr)
    {
        binder_.StartBinding();
        binder_.Connect(contents, &ScrollablePaneContents::Resized, this, &ScrollablePane::ContentsResized, Qt::DirectConnection);
    }

    QWidget::showEvent(event);
}

void ScrollablePane::ContentsResized()
{
    updateGeometry();
}
