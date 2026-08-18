// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the SystemTabWidget class that hosts system-level modules.

#ifndef RDP_SOURCE_FRONTEND_SYSTEM_TAB_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SYSTEM_TAB_WIDGET_H_

#include <memory>
#include <unordered_map>

#include <QTabWidget>

struct DevToolsModule;

namespace rdp
{
    /// @brief Widget that hosts system-level module views (e.g. Driver Settings, Device Clocks)
    /// as sub-tabs within the top-level SYSTEM navigation tab.
    ///
    /// Unlike the CaptureTabWidget, this widget has no sidebar, bottom bar, or other
    /// capture-specific scaffolding. System modules operate independently of DevDriver connections.
    class SystemTabWidget : public QTabWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit SystemTabWidget(QWidget* parent = nullptr);

        /// @brief Destructor.
        ~SystemTabWidget() override;

        /// @brief Sets the module model and creates tabs for system-category modules.
        /// @param [in] module_model The model that manages the modules.
        void SetModels(const std::shared_ptr<class ModuleModel>& module_model);

    private:
        /// @brief Adds a system module's view as a sub-tab.
        /// @param [in] module The module to add.
        void AddModuleView(const DevToolsModule* module);

        /// @brief Gets the tab index for a module, or -1 if not present.
        /// @param [in] module The module to look up.
        /// @return The tab index, or -1.
        int GetTabIndexForModule(const DevToolsModule* module) const;

    private:
        std::shared_ptr<class ModuleModel>                  module_model_;  ///< The model that manages the modules.
        std::unordered_map<const DevToolsModule*, QWidget*> module_views_;  ///< Created views for system modules.
    };

}  // namespace rdp

#endif
