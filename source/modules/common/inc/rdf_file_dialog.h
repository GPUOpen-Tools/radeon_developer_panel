// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Dialog class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_RDF_FILE_DIALOG_H_
#define RDP_SOURCE_MODULES_COMMON_INC_RDF_FILE_DIALOG_H_

#include <memory>

#include <QDialog>

namespace Ui
{
    class RdfFileDialog;
}

/// @brief Dialog showing RDF file chunks
class RdfFileDialog : public QDialog
{
public:
    /// @brief Constructor
    /// @param [in] title The dialog title
    /// @param [in] path The rdf file path
    /// @param [in] parent The parent widget
    RdfFileDialog(const QString& title, const QString& path, QWidget* parent = nullptr);

    /// @brief Destructor
    ~RdfFileDialog() = default;

protected:
    /// @brief Handles incremementing instance count used for display offset
    /// @param [in] event The show event.
    void showEvent(QShowEvent* event) Q_DECL_OVERRIDE;

    /// @brief Handles decrementing the instance count used for display offset
    /// @param [in] event The close event.
    void closeEvent(QCloseEvent* event) Q_DECL_OVERRIDE;

    /// @brief Handles detecting key press for closing dialog through key press
    /// @param [in] event The key event.
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

private:
    std::unique_ptr<Ui::RdfFileDialog> ui_;  ///< Qt ui

    static uint32_t instance_count_;  ///< Global instance count for position offset
};

#endif
