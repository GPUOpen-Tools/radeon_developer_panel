// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the blocklist widget.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_BLOCKLIST_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_BLOCKLIST_WIDGET_H_

#include <memory>

#include <QMenu>
#include <QSortFilterProxyModel>
#include <QString>

#include "collapsible_pane.h"
#include "executable_list_widget.h"

#include "models/blocklist_model.h"

class CollapsiblePaneButton;

namespace rdp
{
    /// @brief Widget that displays a list of applications.
    class BlocklistListWidget : public ExecutableListWidget, public CollapsiblePaneButtonProvider
    {
        Q_OBJECT

        /// @brief Proxy model for the blocklist model.
        class BlocklistProxyModel : public QSortFilterProxyModel
        {
        public:
            /// @brief Constructor.
            /// @param [in] parent Parent object if any.
            BlocklistProxyModel(QObject* parent = nullptr);

            bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const override;
        };

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit BlocklistListWidget(QWidget* parent = nullptr);

        void CreateButtons(std::list<QAbstractButton*>& buttons) override;

        /// @brief Sets the model for this widget.
        /// @param [in] model The model to use for this widget.
        void SetModel(const std::shared_ptr<BlocklistModel>& model);

    signals:
        /// @brief Emitted when the name of the platform changes.
        ///
        /// Empty string is unknown / disconnected.
        /// @param [in] name The name of the platform.
        void PlatformNameChanged(const QString& name);

    private slots:
        /// @brief Called when the platform for the blocklist changes.
        /// @param [in] platform The new platform.
        void OnPlatformChanged(BlocklistModel::Platform platform);

        /// @brief Called when the add button is pressed and presents a dialog to add an application to the blocklist.
        void AddApplication();

        /// @brief Called when the context menu is requested.
        /// @param [in] pos The position where the context menu was requested.
        void ContextMenuRequested(const QPoint& pos);

        /// @brief Handle editing list entry
        void OnEditItem();

        /// @brief Handle removing list entry
        void OnRemoveItem();

    private:
        std::shared_ptr<class BlocklistModel> model_;        ///< The model for this widget.
        std::unique_ptr<BlocklistProxyModel>  proxy_model_;  ///< The proxy model for this widget.

        QMenu context_menu_;  ///< Context menu for the blocklist.

        CollapsiblePaneButton* add_button_;    ///< Button to add items.
        CollapsiblePaneButton* reset_button_;  ///< Button to reset blocklist.
    };

    using CollapsibleBlocklistWidget = ButtonCollapsiblePane<BlocklistListWidget>;
}  // namespace rdp

#endif
