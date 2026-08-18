// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for BottomBatTabWidget class.

#include "bottom_bar_tab_widget.h"

#include <QPushButton>
#include <QVBoxLayout>

#include <common/inc/util.h>
#include "logging/logging_manager.h"

namespace rdp
{
    static constexpr int kMinimizeButtonFixedHeight  = 25;
    static constexpr int kMinimizeButtonRightPadding = 4;
    static constexpr int kContentsMargin             = 12;

    CollapsibleTabWidget::CollapsibleTabWidget(QWidget* parent)
        : QTabWidget(parent)
    {
    }

    void CollapsibleTabWidget::SetCollapsed(bool is_collapsed)
    {
        if (!styleSheet().isEmpty() == is_collapsed)
        {
            return;
        }

        // When the bar is collapsed, we want to hide the bottom border. However, doing so ruins the border for when it is expanded, so we need to leave
        // it there so that the rest of the pane looks good.
        setStyleSheet(is_collapsed ? "QTabWidget::pane {border-bottom: none;}" : "");
    }

    int CollapsibleTabWidget::GetVerticalPadding() const
    {
        QStyleOptionTabWidgetFrame option;
        initStyleOption(&option);
        option.state = QStyle::State_None;

        const QSize padding = style()->sizeFromContents(QStyle::CT_TabWidget, &option, {}, this);
        return padding.height();
    }

    BottomBarTabWidget::BottomBarTabWidget(BottomBarTabWidgetDelegate* delegate, const std::shared_ptr<SystemInfoModel>& sys_info_model, QWidget* parent)
        : QWidget(parent)
        , delegate_(delegate)
    {
        tab_widget_ = new CollapsibleTabWidget(this);
        tab_widget_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);

        QFont font = tab_widget_->font();
        font.setBold(true);
        tab_widget_->setFont(font);

        QVBoxLayout* main_layout = new QVBoxLayout(this);
        main_layout->addWidget(tab_widget_);

        main_layout->setSpacing(0);
        main_layout->setContentsMargins(0, 0, 0, 0);
        setLayout(main_layout);

        minimize_button_ = new QToolButton(this);
        minimize_button_->setFixedHeight(kMinimizeButtonFixedHeight);
        minimize_button_->setStyleSheet("QToolButton{ border: none;}");
        minimize_button_->setIcon(QIcon(":/minus.svg"));

        QSizePolicy policy = minimize_button_->sizePolicy();
        policy.setRetainSizeWhenHidden(true);
        minimize_button_->setSizePolicy(policy);

        QHBoxLayout* corner_layout = new QHBoxLayout(corner_widget_);
        corner_layout->addWidget(minimize_button_);

        corner_widget_ = new QWidget(this);
        corner_widget_->setLayout(corner_layout);
        tab_widget_->setCornerWidget(corner_widget_);

        system_info_widget_ = new SystemInfoWidget(this);
        debug_log_widget_   = new DebugLogWidget(LoggingManager::Instance().GetLoggingModel(), this);

        AddTab(system_info_widget_, "System information");
        AddTab(debug_log_widget_, "Output log");

        // This hidden tab is used to make it look like all the buttons in the tab bar are unselected when it is collapsed.
        hidden_tab_index = tab_widget_->addTab(new QWidget(this), "");
        tab_widget_->setTabVisible(hidden_tab_index, false);
        tab_widget_->setCurrentIndex(hidden_tab_index);

        connect(tab_widget_, &QTabWidget::tabBarClicked, this, &BottomBarTabWidget::OnCurrentTabChanged);
        connect(minimize_button_, &QPushButton::pressed, this, &BottomBarTabWidget::MinimizeButtonPressed);

        connect(sys_info_model.get(), &SystemInfoModel::Loaded, system_info_widget_, &SystemInfoWidget::OnSystemInfoModelLoaded);
    }

    void BottomBarTabWidget::AddTab(QWidget* widget, QString tab_name)
    {
        QWidget*     container       = new QWidget(this);
        QVBoxLayout* vertical_layout = new QVBoxLayout(container);

        vertical_layout->setContentsMargins(kContentsMargin, kContentsMargin, kContentsMargin, kContentsMargin);
        container->setLayout(vertical_layout);
        vertical_layout->addWidget(widget);

        tab_widget_->addTab(container, tab_name);
    }

    QSize BottomBarTabWidget::minimumSizeHint() const
    {
        const QSize tab_size_hint  = tab_widget_->minimumSizeHint();
        const int   tab_bar_height = tab_widget_->tabBar()->minimumSizeHint().height() - tab_widget_->GetVerticalPadding();

        return {tab_size_hint.width(), collapsed_ ? tab_bar_height : tab_size_hint.height()};
    }

    void BottomBarTabWidget::resizeEvent(QResizeEvent* event)
    {
        QWidget::resizeEvent(event);
        corner_widget_->layout()->setContentsMargins(
            0, 0, kMinimizeButtonRightPadding, (tab_widget_->tabBar()->height() - tab_widget_->GetVerticalPadding() - kMinimizeButtonFixedHeight) / 2);
    }

    bool BottomBarTabWidget::IsCollapsed() const
    {
        return collapsed_;
    }

    void BottomBarTabWidget::SetCollapsed(bool is_collapsed, bool force)
    {
        if (collapsed_ == is_collapsed && !force)
        {
            return;
        }

        tab_widget_->SetCollapsed(is_collapsed);
        collapsed_ = is_collapsed;

        if (is_collapsed)
        {
            minimize_button_->hide();
            tab_widget_->setCurrentIndex(hidden_tab_index);

            setMaximumHeight(minimumSizeHint().height());
            delegate_->SetBottomBarHeight(maximumHeight());

            return;
        }

        minimize_button_->show();

        Q_ASSERT(tab_widget_->count() > 0);
        tab_widget_->setCurrentIndex(cached_tab_index_ == -1 ? 0 : cached_tab_index_);

        setMaximumHeight(QWIDGETSIZE_MAX);
        delegate_->SetBottomBarHeight(minimumSizeHint().height());
    }

    void BottomBarTabWidget::MinimizeButtonPressed()
    {
        if (IsCollapsed())
        {
            return;
        }

        SetCollapsed(true);
    }

    void BottomBarTabWidget::OnCurrentTabChanged(int index)
    {
        cached_tab_index_ = index;

        if (!IsCollapsed())
        {
            return;
        }

        SetCollapsed(false);
    }

    void BottomBarTabWidget::SplitterDraggedPastMinSizeInDirection(int direction)
    {
        if (collapsed_ && direction > 0)
        {
            SetCollapsed(false);
        }

        if (!collapsed_ && direction < 0)
        {
            SetCollapsed(true);
        }
    }
}  // namespace rdp
