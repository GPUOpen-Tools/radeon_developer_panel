// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the available modules widget delegate.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_AVAILABLE_MODULES_DELEGATE_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_AVAILABLE_MODULES_DELEGATE_H_

#include "no_focus_delegate.h"

namespace rdp
{
    /// @brief The item delegate to use for the available modules widget.
    class AvailableModulesItemDelegate : public NoFocusDelegate
    {
        Q_OBJECT
    public:
        static constexpr int kRowHeight = 46;  ///< The height of each row.

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent object.
        explicit AvailableModulesItemDelegate(QObject* parent = nullptr);

        /// @brief Paints an item in the list.
        /// @param [in] painter The object that can perform painting.
        /// @param [in] option The options for the item to paint with.
        /// @param [in] index The index of the item to paint.
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        /// @brief Gets the size hint for an item in the list, but uses a fixed height of kRowHeight.
        /// @param [in] option The options for the item to get the size hint for.
        /// @param [in] index The index of the item to get the size hint for.
        /// @return
        QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        /// @brief Handles editor events and emits PressedEnable() when the enable button is clicked for a module.
        /// @param [in] event The editor event.
        /// @param [in] model The current model of the list item.
        /// @param [in] option The options for the item that received the event.
        /// @param [in] index The index of the item that received the event.
        /// @return true, since all events will be consumed.
        bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;

        /// @brief Handles the help event and shows a tooltip over the enable button if the module is incompatible.
        /// @param [in] event The help event.
        /// @param [in] view The view the help event is for.
        /// @param [in] option The options for the item that received the event.
        /// @param [in] index The index of the item that received the event.
        /// @return true if the event should be consumed, false otherwise.
        bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override;

    signals:

        /// @brief Emitted when enable is pressed for one of the available modules.
        /// @param [in] index The index of the module that the enable button was pressed for.
        void PressedEnable(const QModelIndex& index);

    private:
        /// @brief Gets the rect to draw the enable button in based on the rect for the whole item.
        /// @param [in] item_rect The rect to draw the list item in.
        /// @param [in] button_size The size (height and width) of the button that should be drawn in the rect.
        /// @param [in] font_metrics The font metrics.
        /// @return The rect to draw the enable button in.
        static QRect GetEnabledButtonRect(const QRect& item_rect, int button_size, const QFontMetrics& font_metrics);

        /// @brief Gets the vertical padding for the enable button.
        /// @param [in] font_metrics The font metrics.
        /// @return The vertical padding for the enable button.
        static int GetEnableButtonVerticalPadding(const QFontMetrics& font_metrics);

    public:
        /// @brief Sets whether the modules are locked or not.
        ///
        /// This will require a repaint to take effect.
        /// @param [in] are_modules_locked true if the modules are locked, false otherwise.
        void SetModulesLocked(bool are_modules_locked);

    private:
        QIcon enable_button_icon_;           ///< The icon to use for the enable button.
        QIcon enable_button_icon_disabled_;  ///< The icon to use for the enable button when it is disabled.
        QIcon warning_icon_;                 ///< The icon to use to show a warning.

        bool are_modules_locked_ = false;  ///< true if the modules are locked, false otherwise.
    };
}  // namespace rdp

#endif
