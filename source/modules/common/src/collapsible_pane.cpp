// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for a collapsible pane widget.

#include "collapsible_pane.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>

CollapsiblePaneBase::CollapsiblePaneBase(QWidget* parent)
    : QFrame(parent)
{
    connect(&QtCommon::QtUtils::ColorTheme::Get(), &QtCommon::QtUtils::ColorTheme::ColorThemeUpdated, this, &CollapsiblePaneBase::OnColorThemeChanged);

    const ColorThemeType current_theme = QtCommon::QtUtils::ColorTheme::Get().GetColorTheme();
    UpdateIconNames(current_theme);
}

void CollapsiblePaneBase::RequestToggle()
{
    Toggle();
}

void CollapsiblePaneBase::UpdateIconNames(ColorThemeType theme)
{
    if (theme == kColorThemeTypeCount)
    {
        theme = QtCommon::QtUtils::DetectOsSetting();
    }

    if (theme == kColorThemeTypeDark)
    {
        chevron_icon_down_  = "chevron-down-dm.svg";
        chevron_icon_right_ = "chevron-right-dm.svg";
    }
    else
    {
        chevron_icon_down_  = "chevron-down.svg";
        chevron_icon_right_ = "chevron-right.svg";
    }
}

void CollapsiblePaneBase::OnColorThemeChanged()
{
    const ColorThemeType current_theme = QtCommon::QtUtils::ColorTheme::Get().GetColorTheme();
    UpdateIconNames(current_theme);

    UpdateChevron();
}

ClickableWidget::ClickableWidget(QWidget* parent)
    : QWidget(parent)
{
}

void ClickableWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit Pressed();
        return;
    }

    QWidget::mousePressEvent(event);
}
