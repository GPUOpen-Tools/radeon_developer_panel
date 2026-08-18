// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for BottomBatTabWidget class.

#ifndef RDP_SOURCE_FRONTEND_BOTTOM_BAR_TAB_WIDGET_H_
#define RDP_SOURCE_FRONTEND_BOTTOM_BAR_TAB_WIDGET_H_

#include <optional>

#include <QApplication>
#include <QSettings>
#include <QStackedWidget>
#include <QStyleOptionTabWidgetFrame>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>

#include "logging/debug_log_widget.h"
#include "system_info_widget.h"

namespace rdp
{
    /// @brief Tab widget that can be collapsed.
    class CollapsibleTabWidget : public QTabWidget
    {
    public:
        /// @brief Constructor.
        /// @param [in] parent Parent widget if any.
        explicit CollapsibleTabWidget(QWidget* parent = nullptr);

        /// @brief Set the collapsed flag
        /// @param [in] is_collapsed The new collapsed flag value
        void SetCollapsed(bool is_collapsed);

        /// @brief Provides the height of the padding that is put around the current tab on the top and bottom.
        /// @return The vertical padding around the current tab widget.
        [[nodiscard]] int GetVerticalPadding() const;
    };

    /// @brief Delegate for the bottom bar tab widget.
    class BottomBarTabWidgetDelegate
    {
    public:
        /// @brief Destructor.
        ~BottomBarTabWidgetDelegate() = default;

        /// @brief Sets the height of the bottom bar tab widget.
        /// @param [in] new_height The new height of the bottom bar tab widget.
        virtual void SetBottomBarHeight(int new_height) = 0;
    };

    /// @brief Collapsible tab widget for the bottom bar of RDP.
    class BottomBarTabWidget : public QWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] sys_info_model The system info model.
        /// @param [in] amd_alert_model The logging model for AMD alert.
        /// @param [in] parent The parent widget.
        BottomBarTabWidget(BottomBarTabWidgetDelegate* delegate, const std::shared_ptr<SystemInfoModel>& sys_info_model, QWidget* parent = nullptr);

    private:
        /// @brief Adds a tab.
        /// @param [in] widget The widget to add.
        /// @param [in] tab_name The name of the tab.
        void AddTab(QWidget* widget, QString tab_name);

    public:
        QSize minimumSizeHint() const override;
        void  resizeEvent(QResizeEvent* event) override;

    public:
        /// @brief Returns if the widget is collapsed
        bool IsCollapsed() const;

    public:
        /// @brief Set the collapsed flag
        /// @param [in] is_collapsed The new collapsed flag value
        /// @param [in] force true if the bar should be forced to collapse or expand even if it is already in the state specified by is_collapsed.
        void SetCollapsed(bool is_collapsed, bool force = false);

    private slots:
        /// @brief Minimizes the tab bar.
        void MinimizeButtonPressed();

        /// @brief Called when the current tab changes and expands the tab bar if needed.
        /// @param [in] index The index of the selected tab.
        void OnCurrentTabChanged(int index);

    public slots:
        /// @brief Called when the splitter is dragged past the minimum size of a widget.
        /// @param [in] direction The direction the splitter is being dragged.
        void SplitterDraggedPastMinSizeInDirection(int direction);

    private:
        BottomBarTabWidgetDelegate* delegate_ = nullptr;  ///< Delegate for this bottom bar.

        CollapsibleTabWidget* tab_widget_      = nullptr;  ///< The tab bar.
        QWidget*              corner_widget_   = nullptr;  ///< the corner widget.
        QToolButton*          minimize_button_ = nullptr;  ///< The button that allows the bar to be minimized.

        DebugLogWidget*   debug_log_widget_   = nullptr;  ///< Debug log widget.
        SystemInfoWidget* system_info_widget_ = nullptr;  ///< System info widget.

        bool collapsed_        = true;  ///< true if the widget is collapsed, false otherwise.
        int  cached_tab_index_ = -1;    ///< The last tab that the bar was shown with.
        int  hidden_tab_index  = -1;    ///< The index of the hidden tab.
    };
}  // namespace rdp

#endif
