// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definition for the available modules widget.

#ifndef RDP_SOURCE_FRONTEND_SIDEBAR_AVAILABLE_MODULES_WIDGET_H_
#define RDP_SOURCE_FRONTEND_SIDEBAR_AVAILABLE_MODULES_WIDGET_H_

#include <memory>

#include <QListView>
#include <QSortFilterProxyModel>

#include "../tight_list_view.h"
#include "collapsible_pane.h"

class CollapsiblePaneButton;

namespace rdp
{
    /// @brief The widget that displays the available modules.
    class AvailableModulesWidget : public QWidget, public CollapsiblePaneButtonProvider
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] parent The parent widget.
        explicit AvailableModulesWidget(QWidget* parent = nullptr);

        void CreateButtons(std::list<QAbstractButton*>& buttons) override;

    public:
        /// @brief Sets the model for this widget.
        /// @param [in] module_model The module that manages the modules.
        /// @param [in] preset_model The model that manages presets.
        void SetModuleModel(const std::shared_ptr<class ModuleModel>& module_model, const std::shared_ptr<class PresetModel>& preset_model);

    private slots:
        /// @brief Enables the module at the specified index.
        /// @param [in] index The index of the module to enable.
        void EnableModule(const QModelIndex& index);

        /// @brief Called when a row is removed.
        /// @param [in] parent The parent of the section where removal is taking place.
        /// @param [in] first The start index of the removed chunk.
        /// @param [in] last The end index of the removed chunk.
        void OnRowRemoved(const QModelIndex& parent, int first, int last);

        /// @brief Called when a row is removed.
        void OnRowInserted();

    private slots:
        /// @brief Called when modules are locked or unlocked.
        void UpdateModulesLocked();

    private slots:
        /// @brief Opens the context menu to select the different options.
        void OpenContextMenu();

    private:
        /// @brief Opens the dialog to load a preset.
        void LoadPreset();

        /// @brief Opens the dialog to save a preset.
        void SavePreset();

        /// @brief Opens the dialog to delete a preset.
        void DeletePreset();

    private:
        std::shared_ptr<class ModuleModel> module_model_;  /// < The model that manages the modules.
        std::shared_ptr<class PresetModel> preset_model_;  ///< The model that manages the presets.

        /// @brief Tight list view with 0 minimum height.
        class AvailableModulesTightList : public TightListView
        {
        public:
            /// @brief Constructor.
            /// @param [in] parent The parent widget.
            AvailableModulesTightList(QWidget* parent = nullptr);

            /// @brief Gets the absolute minimum height for this view.
            /// @return The absolute minimum height for this view.
            virtual int GetMinHeight() const;
        };

        /// @brief Proxy model used to show only modules that are not enabled.
        class AvailableModulesProxyModel : public QSortFilterProxyModel
        {
        protected:
            /// @brief Returns true if the module at the given row is not enabled.
            /// @param [in] source_row The row to decide whether or not it should be included.
            /// @param source_parent The parent of the row.
            /// @return true if the row should be displayed, false otherwise.
            bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;
        };

        std::shared_ptr<AvailableModulesProxyModel> proxy_model_;       ///< The proxy model that wraps the module model.
        std::shared_ptr<AvailableModulesTightList>  list_view_;         ///< The list view that displays the available modules.
        std::shared_ptr<class QLabel>               no_modules_label_;  ///< The label that says there are no modules.

        std::shared_ptr<class AvailableModulesItemDelegate> list_delegate_;  ///< The item delegate for the list view.

        CollapsiblePaneButton* preset_button_ = nullptr;  ///< The preset button.
    };

    using CollapsibleAvailableModulesWidget = ButtonCollapsiblePane<AvailableModulesWidget>;
}  // namespace rdp

#endif
