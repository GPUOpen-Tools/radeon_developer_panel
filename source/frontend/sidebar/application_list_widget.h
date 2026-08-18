// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the application list widget.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_APPLICATION_LIST_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_APPLICATION_LIST_WIDGET_H_

#include <memory>

#include "collapsible_pane.h"
#include "common/inc/rdp_combo_box.h"
#include "executable_list_widget.h"
#include "models/api_model.h"
#include "models/application_model.h"

namespace rdp
{

    /// @brief Widget that displays a list of applications.
    class ApplicationListWidget final : public ExecutableListWidget, public CollapsiblePaneButtonProvider
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit ApplicationListWidget(QWidget* parent = nullptr);

        void CreateButtons(std::list<QAbstractButton*>& buttons) override;

        /// @brief Sets the models for this widget.
        /// @param [in] application_model The model to use for this widget.
        /// @param [in] blocklist_model The model that manages the blocklist.
        /// @param [in] connection_model The model that manages the connection to RDS.
        /// @param [in] api_proxy_model The API proxy model.
        void SetModels(const std::shared_ptr<class ApplicationModel>& application_model,
                       const std::shared_ptr<class BlocklistModel>&   blocklist_model,
                       const std::shared_ptr<class ConnectionModel>&  connection_model,
                       const std::shared_ptr<class ApiProxyModel>&    api_proxy_model);

    private slots:
        /// @brief Called when the number of rows changes.
        void RowsInserted();

        /// @brief Called when the number of rows changes.
        void RowNumberChanged();

        /// @brief Called when an application connects.
        /// @param [in] index The application entry index from the application model.
        void OnApplicationConnected(const QModelIndex& index) const;

        /// @brief Forces the list view to repaint.
        void ForceRepaint() const;

        /// @brief Called when the add button is pressed and presents a dialog to add an application.
        void AddApplication();

        /// Called when the selected application changes.
        /// @param [in] index The index of the selected application.
        void SelectionChanged(const QModelIndex& index) const;

        /// @brief Called when data changes in the application model.
        void DataChanged() const;

        /// @brief Called when the context menu is requested.
        /// @param [in] pos The position where the context menu was requested.
        void ContextMenuRequested(const QPoint& pos) const;

        /// @brief Called when trying to edit an application name, but the application name was on the blocklist.
        /// @param [in] app_name The application name.
        void EditedAppNameButAlreadyOnBlocklist(const QString& app_name);

        /// @brief Handles the auto connection mode selected index changing.
        /// @param [in] index The new index.
        void HandleAutoConnectComboBoxChanged(int index) const;

        /// @brief Handles response to user selection in API filter.
        /// @param [in] index The selected index
        void HandleApiFilterChosen(int index) const;

        /// @brief Called when the API filter changes.
        /// @param [in] new_filter The new API filter.
        void OnApiFilterChanged(ApiModel::Api new_filter) const;

        /// @brief Called when the auto connection mode changes.
        /// @param [in] mode The new auto connection mode.
        void OnAutoConnectionModeChanged(ApplicationAutoConnectMode mode) const;

        /// @brief Called when the API count of the source model changes.
        /// @param [in] count The new number of available APIs.
        void SourceApiCountChanged(int count) const;

    private:
        std::shared_ptr<ApplicationModel>            application_model_;                 ///< The application model.
        std::shared_ptr<BlocklistModel>              blocklist_model_;                   ///< The application model.
        std::shared_ptr<ConnectionModel>             connection_model_;                  ///< The application model.
        std::shared_ptr<class ApiProxyModel>         api_proxy_model_;                   ///< The API proxy model.
        std::unique_ptr<class ApplicationProxyModel> proxy_model_;                       ///< Proxy that wraps the application model.
        std::shared_ptr<class AutoConnectModel>      auto_connect_model_;                ///< The model used for the auto connection mode.
        QLabel*                                      desc_label_             = nullptr;  ///< The label for the app filter description.
        QLabel*                                      auto_connect_label_     = nullptr;  ///< The label for auto connect combo box.
        QLabel*                                      api_label_              = nullptr;  ///< The label for API combo box.
        RdpComboBox*                                 auto_connect_combo_box_ = nullptr;  ///< The auto connect combo box.
        RdpComboBox*                                 api_combo_box_          = nullptr;  ///< The API combo box.
    };

    using CollapsibleApplicationListWidget = ButtonCollapsiblePane<ApplicationListWidget>;
}  // namespace rdp

#endif
