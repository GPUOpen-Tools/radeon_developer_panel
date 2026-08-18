// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the save preset dialog.

#ifndef RDP_SOURCE_FRONTEND_PRESETS_SAVE_PRESET_DIALOG_H_
#define RDP_SOURCE_FRONTEND_PRESETS_SAVE_PRESET_DIALOG_H_

#include <memory>

#include <QDialog>

namespace Ui
{
    class SavePresetDialog;
}

namespace rdp
{
    /// @brief The dialog to save a preset.
    class SavePresetDialog : public QDialog
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] preset_model The model responsible for managing presets.
        /// @param [in] widget The parent of this widget.
        explicit SavePresetDialog(const std::shared_ptr<class PresetModel>& preset_model, QWidget* widget = nullptr);

        /// @brief Destructor.
        ~SavePresetDialog() override;

        void showEvent(QShowEvent* event) override;

    private slots:
        /// @brief Called when the selected item in the combo box changes.
        /// @param [in] index The index of the newly selected item.
        void PresetComboBoxChanged(int index);

        /// @brief Called when the text for the custom preset input is updated.
        /// @param [in] text The text for the custom preset input.
        void CustomPresetNameChanged(const QString& text);

        /// @brief Called when the user presses cancel.
        void Cancel();

        /// @brief Called when the user presses ok - saves the preset.
        void Accept();

    private:
        std::unique_ptr<Ui::SavePresetDialog> ui_;            ///< The ui for the dialog.
        std::shared_ptr<class PresetModel>    preset_model_;  ///< The model responsible for managing presets.
    };
}  // namespace rdp

#endif
