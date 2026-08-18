// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the add application dialog.

#ifndef RDP_SOURCE_FRONTEND_PRESETS_ADD_APPLICATION_DIALOG_H_
#define RDP_SOURCE_FRONTEND_PRESETS_ADD_APPLICATION_DIALOG_H_

#include <memory>

#include <QDialog>

namespace Ui
{
    class AddApplicationDialog;
}

namespace rdp
{
    /// @brief The dialog to add an application.
    class AddApplicationDialog : public QDialog
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] blocklist_model If not nullptr, applications that are on the list will not be allowed.
        /// @param [in] widget The parent of this widget.
        explicit AddApplicationDialog(const std::shared_ptr<class BlocklistModel>& blocklist_model = nullptr, QWidget* widget = nullptr);

        /// @brief Destructor.
        ~AddApplicationDialog() override;

    signals:
        /// @brief Adds the application.
        /// @param [in] application_name The name of the application.
        void AddApplication(const QString& application_name);

    private slots:
        /// @brief Called when the app name text changes.
        /// @param [in] text The new text.
        void AppTextChanged(const QString& text);

        /// @brief Called when the user presses cancel.
        void Cancel();

        /// @brief Called when the user presses ok - emits AddApplication() and closes.
        void Accept();

    private:
        std::unique_ptr<Ui::AddApplicationDialog> ui_;               ///< The ui for the dialog.
        std::shared_ptr<class BlocklistModel>     blocklist_model_;  ///< The model to use to check the blocklist against.
    };
}  // namespace rdp

#endif
