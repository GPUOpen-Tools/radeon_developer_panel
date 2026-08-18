// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the SystemTabWidget class that hosts system-level modules.

#include "system_tab_widget.h"

#include <common/inc/api/dev_tools_module.h>

#include "models/module_model.h"

namespace rdp
{
    SystemTabWidget::SystemTabWidget(QWidget* parent)
        : QTabWidget(parent)
    {
        setTabsClosable(false);
    }

    SystemTabWidget::~SystemTabWidget()
    {
        // Remove all tabs first so the internal QStackedWidget no longer tracks
        // these widgets — prevents crash from current-widget adjustments during
        // deletion (same issue as CaptureTabWidget).
        while (count() > 0)
        {
            removeTab(0);
        }

        for (const auto& pair : module_views_)
        {
            delete pair.second;
        }

        module_views_.clear();
    }

    void SystemTabWidget::SetModels(const std::shared_ptr<ModuleModel>& module_model)
    {
        module_model_ = module_model;

        // Create views for all system-category modules. System modules are always
        // visible in the SYSTEM tab — they are not gated by the enable/disable workflow.
        for (const DevToolsModule* module : module_model_->GetModules())
        {
            if (module->GetModuleCategory() != ModuleCategory::kSystem)
            {
                continue;
            }

            module_views_.insert(std::make_pair(module, module->CreateView()));
            AddModuleView(module);
        }

        // Default to the first tab (Driver Settings is loaded first).
        if (count() > 0)
        {
            setCurrentIndex(0);
        }
    }

    void SystemTabWidget::AddModuleView(const DevToolsModule* module_to_add)
    {
        if (module_model_ == nullptr || module_views_.count(module_to_add) != 1)
        {
            return;
        }

        // Insert in module order to maintain consistent tab ordering.
        int insertion_index = 0;
        for (const DevToolsModule* module : module_model_->GetModules())
        {
            if (module->GetModuleCategory() != ModuleCategory::kSystem)
            {
                continue;
            }

            if (module_to_add == module)
            {
                insertTab(insertion_index, module_views_[module], module->GetModuleDisplayName().c_str());
                break;
            }

            if (GetTabIndexForModule(module) != -1)
            {
                ++insertion_index;
            }
        }
    }

    int SystemTabWidget::GetTabIndexForModule(const DevToolsModule* module) const
    {
        auto it = module_views_.find(module);
        if (it == module_views_.end())
        {
            return -1;
        }

        QWidget* module_view = it->second;
        for (int tab = 0; tab < count(); ++tab)
        {
            if (widget(tab) == module_view)
            {
                return tab;
            }
        }

        return -1;
    }

}  // namespace rdp
