// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Application context menu class definition

#ifndef RDP_SOURCE_FRONTEND_APPLICATION_CONTEXT_MENU_H_
#define RDP_SOURCE_FRONTEND_APPLICATION_CONTEXT_MENU_H_

#include <QMenu>

#include "application.h"
#include "models/application_model.h"
#include "models/blocklist_model.h"

namespace rdp
{
    /// @brief Custom context menu for application entries
    class ApplicationContextMenu : public QMenu
    {
        Q_OBJECT
    public:
        /// @brief Constructor
        /// @param [in] index The application model index
        /// @param [in] application_model The application model
        /// @param [in] blocklist_model The blocklist model.
        /// @param [in] enable_blocking True if blocking an application should be enabled, false otherwise.
        /// @param [in] parent The parent widget
        ApplicationContextMenu(QModelIndex                              index,
                               const std::shared_ptr<ApplicationModel>& application_model,
                               std::shared_ptr<BlocklistModel>          blocklist_model,
                               bool                                     enable_blocking,
                               QWidget*                                 parent = nullptr);

        /// @brief Destructor
        ~ApplicationContextMenu() override;

    private slots:
        /// @brief Handle remove action
        void OnRemove();

        /// @brief Handle add to blocklist
        void OnAddToBlocklist();

    private:
        QModelIndex                       index_;              ///< The application model index
        std::shared_ptr<ApplicationModel> application_model_;  ///< The application model
        std::shared_ptr<BlocklistModel>   blocklist_model_;    ///< The blocklist model
    };

}  // namespace rdp

#endif
