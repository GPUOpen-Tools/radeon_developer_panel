// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for sidebar widget.

#include "sidebar_widget.h"

#include <algorithm>

#include <QLabel>
#include <QScrollBar>

#include "models/sidebar/auto_connect_model.h"

namespace rdp
{

    /// @brief The additional margin that is added on top of the space required to fit the scroll bar.
    static constexpr int kSidebarHorizontalMarginPadding = 3;
    static constexpr int kSidebarVerticalMargin          = 20;

    static constexpr int kCollapsiblePaneSpacing = 15;

    SidebarWidget::SidebarWidget(QWidget* parent)
        : ScrollablePane(parent)
    {
        setFrameStyle(QFrame::NoFrame);
        setWidgetResizable(true);

        setObjectName("sidebar");

        contents_ = new ScrollablePaneContents();
        contents_->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        setWidget(contents_);

        content_layout_ = new QVBoxLayout(contents_);
        content_layout_->setAlignment(Qt::AlignTop);
        content_layout_->setSpacing(kCollapsiblePaneSpacing);
        contents_->setLayout(content_layout_);

        PopulateSidebar();
    }

    void SidebarWidget::PopulateSidebar()
    {
        // These are all just placeholders until the actual widgets become available
        available_modules_widget_ = new CollapsibleAvailableModulesWidget(contents_);
        available_modules_widget_->SetTitleText("Available features");
        content_layout_->addWidget(available_modules_widget_);

        application_list_ = new CollapsibleApplicationListWidget(contents_);
        application_list_->SetTitleText("Applications");
        content_layout_->addWidget(application_list_);

        blocklist_ = new CollapsibleBlocklistWidget(contents_);
        blocklist_->Collapse();

        connect(blocklist_->GetBody(), &BlocklistListWidget::PlatformNameChanged, this, &SidebarWidget::BlocklistPlatformNameChanged);
        BlocklistPlatformNameChanged("");

        content_layout_->addWidget(blocklist_);
    }

    void SidebarWidget::SetModels(const std::shared_ptr<ApiProxyModel>&     api_proxy_model,
                                  const std::shared_ptr<ApplicationModel>&  application_model,
                                  const std::shared_ptr<BlocklistModel>&    blocklist_model,
                                  const std::shared_ptr<ConnectionModel>&   connection_model,
                                  const std::shared_ptr<ModuleModel>&       module_model,
                                  const std::shared_ptr<class PresetModel>& preset_model)
    {
        application_list_->GetBody()->SetModels(application_model, blocklist_model, connection_model, api_proxy_model);
        available_modules_widget_->GetBody()->SetModuleModel(module_model, preset_model);
        blocklist_->GetBody()->SetModel(blocklist_model);
    }

    void SidebarWidget::BlocklistPlatformNameChanged(const QString& name)
    {
        const QString title = name.isEmpty() ? "Blocklist" : QString("Blocklist [%1]").arg(name);
        blocklist_->SetTitleText(title);
    }
}  // namespace rdp
