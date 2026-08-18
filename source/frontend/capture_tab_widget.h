// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the widget that shows all of the module tabs with the client views.

#ifndef RDP_SOURCE_FRONTEND_CAPTURE_TAB_WIDGET_H_
#define RDP_SOURCE_FRONTEND_CAPTURE_TAB_WIDGET_H_

#include <map>
#include <memory>

#include <QTabWidget>
#include <QWidget>

struct DevToolsModule;

namespace rdp
{
    /// @brief Widget that manages the tabs for all of the module tabs with the client views.
    class CaptureTabWidget : public QTabWidget
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit CaptureTabWidget(QWidget* parent = nullptr);

        /// @brief Destructor.
        ~CaptureTabWidget();

        /// @brief For whatever reason, this widget will not resize exactly down to its minimum size, so this function
        /// returns the height offset between the minimumSizeHint() and the actual minimum size.
        /// @return The height offset between the minimumSizeHint() and the actual minimum size.
        [[nodiscard]] int GetMinimumHeightOffset() const;

        /// @brief Sets the models for this view.
        /// @param [in] module_model The model that manages the modules.
        /// @param [in] preset_model The model that manages the presets.
        void SetModels(const std::shared_ptr<class ModuleModel>& module_model, const std::shared_ptr<class PresetModel>& preset_model);

    private:
        /// @brief This adds the dummy tab that displays a message that there are no modules.
        ///
        /// In order to still show the hamburger menu, we use some style tricks to add an invisible tab.
        /// This ensures that the UI doesn't change or shift.
        void AddNoModulesTab();

        /// @brief This removes the dummy tab that displays a message that there are no modules.
        ///
        /// In order to still show the hamburger menu, we use some style tricks to add an invisible tab.
        /// This ensures that the UI doesn't change or shift.
        void RemoveNoModulesTab();

        /// @brief Gets the stylesheet for the close button.
        /// @param [in] icon_name The name of the icon to use for the close button.
        /// @return The stylesheet for the close button on each tab.
        static QString GetCloseButtonStyleSheet(const QString& icon_name);

    private slots:

        /// @brief Called when a module's status changes from enabled to disabled or disabled to enabled.
        /// @param [in] module The module that had its status changed.
        /// @param is_enabled true if the module is enabled, false otherwise.
        void OnModuleStatusChanged(const DevToolsModule* module, bool is_enabled);

        /// @brief Called when trying to change a module's status fails.
        /// @param [in] module The module whose status failed to be changed.
        /// @param [in] is_enabled The desired state of the module - true if it should have been enabled, false if it should have been disabled.
        void OnModuleFailedToChangeStatus(const DevToolsModule* module, bool is_enabled);

        /// @brief Respond to color theme updated
        void OnColorThemeChanged();

    private:
        /// @brief Adds a module view to the tab widget.
        /// @param [in] module_to_add The module to add to the tab widget.
        void AddModuleView(const DevToolsModule* module_to_add);

        /// @brief Removes a module view to the tab widget.
        /// @param [in] module The module to remove from the tab widget.
        void RemoveModuleView(const DevToolsModule* module);

        /// @brief Gets the tab index for the module or -1 if the module is not currently being displayed.
        /// @param [in] module The module to get the index for.
        /// @return The tab index for the module or -1 if the module is not currently being displayed.
        int GetTabIndexForModule(const DevToolsModule* module);

        /// @brief Applies special no-modules stylesheet
        void ApplyNoModulesStyle();

    private slots:
        /// @brief Called when the close button is pressed on one of the tabs.
        /// @param [in] index The index of the tab to be closed.
        void CloseTab(int index);

    private slots:
        /// @brief Called when modules are locked.
        void OnModulesLocked();

        /// @brief Called when modules are unlocked.
        void OnModulesUnlocked();

        /// @brief Called when a preset is loaded.
        void OnPresetLoaded();

    private:
        std::shared_ptr<class ModuleModel> module_model_;  ///< The model that manages the modules.
        std::shared_ptr<class PresetModel> preset_model_;  ///< The model that manages the presets.

        std::unordered_map<const DevToolsModule*, QWidget*> module_views_;                ///< The views for each of the modules.
        int                                                 total_module_view_tabs_ = 0;  ///< The total number of module view tabs that are in the widget.
        QString                                             xmark_;                       ///< xmark icon name
    };
}  // namespace rdp

#endif
