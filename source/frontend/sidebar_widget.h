// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Header for sidebar widget.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_WIDGET_H_

#include <QScrollArea>
#include <QVBoxLayout>

#include <common/inc/view/scrollable_pane.h>

#include "sidebar/application_list_widget.h"
#include "sidebar/available_modules/available_modules_widget.h"
#include "sidebar/blocklist_widget.h"

namespace rdp
{

    /// @brief The sidebar widget for the capture tab.
    class SidebarWidget : public ScrollablePane
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit SidebarWidget(QWidget* parent = nullptr);

    private:
        /// @brief Creates the contents of the sidebar and adds them to the layout of the content view.
        void PopulateSidebar();

    public:
        /// @brief Sets all of the models needed for this view.
        /// @param [in] api_proxy_model The API proxy model.
        /// @param [in] application_model The model that manages applications.
        /// @param [in] blocklist_model The model that manages blocked applications.
        /// @param [in] connection_model The model that manages the connection to RDS.
        /// @param [in] module_model The model that manages the modules.
        /// @param [in] preset_model The model that manages the preset_model.
        void SetModels(const std::shared_ptr<class ApiProxyModel>&    api_proxy_model,
                       const std::shared_ptr<class ApplicationModel>& application_model,
                       const std::shared_ptr<class BlocklistModel>&   blocklist_model,
                       const std::shared_ptr<class ConnectionModel>&  connection_model,
                       const std::shared_ptr<class ModuleModel>&      module_model,
                       const std::shared_ptr<class PresetModel>&      preset_model);

    private slots:
        /// @brief Emitted when the name of the platform changes.
        ///
        /// Empty string is unknown / disconnected.
        /// @param [in] name The name of the platform.
        void BlocklistPlatformNameChanged(const QString& name);

    private:
        QWidget*     contents_       = nullptr;  ///< The widget that serves as the content for the scroll area.
        QVBoxLayout* content_layout_ = nullptr;  ///< The layout for the contents_ widget.

        CollapsibleAvailableModulesWidget* available_modules_widget_ = nullptr;  ///< The available modules widget.
        CollapsibleApplicationListWidget*  application_list_         = nullptr;  ///< The application list widget.
        CollapsibleBlocklistWidget*        blocklist_                = nullptr;  ///< The blocklist widget.
    };

};  // namespace rdp

#endif
