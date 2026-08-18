// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for dialog that sets the custom DevDriver timeouts.

#ifndef RDP_SOURCE_FRONTEND_TIMEOUT_DIALOG_H_
#define RDP_SOURCE_FRONTEND_TIMEOUT_DIALOG_H_

#include <memory>

#include <QDialog>

namespace Ui
{
    class TimeoutDialog;
}

namespace rdp
{
    /// @brief Dialog that sets the custom DevDriver timeouts.
    class TimeoutDialog : public QDialog
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] timeout_model The model that handles the custom timeouts.
        TimeoutDialog(const std::shared_ptr<class TimeoutModel>& timeout_model);

        /// @brief Destructor.
        ~TimeoutDialog();

    private slots:

        /// @brief Sets all the timeouts.
        void SaveTimeouts();

        /// @brief Resets all the timeouts.
        void ResetTimeouts();

    private:
        std::shared_ptr<class TimeoutModel> timeout_model_;  ///< The model that handles the custom timeouts.
        std::unique_ptr<Ui::TimeoutDialog>  ui_;             ///< The UI.
    };

}  // namespace rdp

#endif
