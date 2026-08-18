// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for NavigationTabWidget class.

#ifndef RDP_SOURCE_FRONTEND_NAVIGATION_TAB_WIDGET_H_
#define RDP_SOURCE_FRONTEND_NAVIGATION_TAB_WIDGET_H_

#include <QTabBar>
#include <QTabWidget>

namespace rdp
{
    class NavigationTabBar;
    class NavigationTabWidget : public QTabWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] parent The parent widget
        explicit NavigationTabWidget(QWidget* parent = Q_NULLPTR);

        /// Enable or disable tabs.
        /// @param [in] index The tab index to enable or disable
        /// @param [in] enable Set to true to enable or false to disable the tab.
        void SetTabEnabled(int index, bool);

        /// Sets the index of the tab used as a spacer between left justified tabs and
        /// right justified tabs.
        /// @param [in] index The index of the tab used as a spacer.
        void SetSpacerIndex(const int index);

        /// Sets the if the last tab should be used as stretch to take up
        /// maximum space of the parent widget.
        /// @param [in] stretch True is last tab should stretch.
        void SetStretchLast(const bool stretch);

        /// Replaces a tab with a widget (e.g. a toolbar button).
        /// @param [in] index The index on the tab bar to be replaced.
        /// @param [in] tool_widget A pointer to a button widget that will replace the tab.
        /// @param [in] pos The position of the tool widget in tab.
        void SetTabTool(int index, QWidget* tool_widget, QTabBar::ButtonPosition pos = QTabBar::LeftSide);

        /// Returns the QTabBar's height.
        /// @return Height in pixels.
        int TabHeight() const;

    protected:
        /// Returns the TabWidget's QTabBar object.
        /// @return A pointer to the tabBar associated with this TabWidget.
        QTabBar* tabBar() const;

        /// Handle a resize event.
        /// @param [in] resize_event The resize event.
        virtual void resizeEvent(QResizeEvent* resize_event);

    private:
        NavigationTabBar* tab_bar_;  ///< Navigation tab bar.
    };
}  // namespace rdp

#endif
