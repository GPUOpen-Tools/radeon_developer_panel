// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Application context menu class implementation

#include "application_context_menu.h"

#include <utility>
#include "models/blocklist_model.h"

namespace rdp
{
    ApplicationContextMenu::ApplicationContextMenu(QModelIndex                              index,
                                                   const std::shared_ptr<ApplicationModel>& application_model,
                                                   std::shared_ptr<BlocklistModel>          blocklist_model,
                                                   bool                                     enable_blocking,
                                                   QWidget*                                 parent)
        : QMenu(parent)
        , application_model_(application_model)
    {
        // If there is a valid parent, that means there are multiple APIs so we want to look at the top level entry
        index_ = index.parent().isValid() ? index.parent() : index;
        Q_ASSERT(index_.isValid());

        blocklist_model_ = std::move(blocklist_model);

        QAction* add_to_blocklist = addAction("Add to blocklist");
        add_to_blocklist->setEnabled(enable_blocking);
        connect(add_to_blocklist, &QAction::triggered, this, &ApplicationContextMenu::OnAddToBlocklist);

        addSeparator();

        QAction* remove = addAction("Remove");
        connect(remove, &QAction::triggered, this, &ApplicationContextMenu::OnRemove);

        // Check if the application is alive or not. If alive we want
        // to disallow removing or adding to blocklist
        const std::shared_ptr<const Application> application = application_model_->GetApplication(index);
        if (application != nullptr && application->IsAlive())
        {
            add_to_blocklist->setDisabled(true);
            remove->setDisabled(true);
        }
    }

    ApplicationContextMenu::~ApplicationContextMenu() = default;

    void ApplicationContextMenu::OnAddToBlocklist()
    {
        Q_ASSERT(blocklist_model_ != nullptr);

        const QString name = application_model_->GetApplication(index_)->GetName();
        OnRemove();  // Remove application entry

        blocklist_model_->AddApplication(name);
    }

    void ApplicationContextMenu::OnRemove()
    {
        Q_ASSERT(application_model_ != nullptr);

        application_model_->removeRow(index_.row());
    }

}  // namespace rdp
