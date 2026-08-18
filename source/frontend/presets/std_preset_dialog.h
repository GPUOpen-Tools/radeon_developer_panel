// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the standard preset dialog.

#ifndef RDP_SOURCE_FRONTEND_PRESETS_STD_PRESET_DIALOG_H_
#define RDP_SOURCE_FRONTEND_PRESETS_STD_PRESET_DIALOG_H_

#include <memory>

#include <QDialog>

namespace Ui
{
    class StdPresetDialog;
}

namespace rdp
{
    /// @brief Standard preset dialog.
    class StdPresetDialog : public QDialog
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] options The options to display.
        /// @param [in] action_text The text that describes the action the dialog will perform.
        /// @param [in] action The action to perform when a preset is selected.
        /// @param [in] widget The parent of this widget.
        explicit StdPresetDialog(const QStringList&                         options,
                                 const QString&                             action_text,
                                 const std::function<void(const QString&)>& action,
                                 QWidget*                                   widget = nullptr);

        /// @brief Destructor.
        ~StdPresetDialog() override;

        void showEvent(QShowEvent* event) override;

    private slots:
        /// @brief Called when the user presses cancel.
        void Cancel();

        /// @brief Called when the user presses ok - calls the action.
        void Accept();

    private:
        std::unique_ptr<Ui::StdPresetDialog> ui_;      ///< The ui for the dialog.
        std::function<void(const QString&)>  action_;  ///< The action to perform.
    };
}  // namespace rdp

#endif
