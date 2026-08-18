// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Class analysis error dialog declaration.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_ERROR_DIALOG_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_ERROR_DIALOG_H_

#include <memory>

#include <QDialog>

// ReSharper disable once CppInconsistentNaming
namespace Ui
{
    class CrashAnalysisErrorDialog;
}

/// @brief Custom dialog that shows a long error message in a text box.
class CrashAnalysisErrorDialog final : public QDialog
{
    Q_OBJECT
public:
    /// @brief Constructor.
    CrashAnalysisErrorDialog();

    /// @brief Destructor.
    ~CrashAnalysisErrorDialog();

    /// @brief Sets the error message that is displayed in this dialog.
    /// @param [in] error The error message to be displayed in this dialog.
    void SetErrorMessage(const QString& error) const;

private slots:

    /// @brief Called when the Ok button is pressed -- closes the window.
    void OnPressedOkButton();

private:
    std::unique_ptr<Ui::CrashAnalysisErrorDialog> ui_;  ///< The ui for the dialog;
};

#endif
