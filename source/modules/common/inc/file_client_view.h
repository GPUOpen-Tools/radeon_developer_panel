// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief File based client view class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_FILE_CLIENT_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_FILE_CLIENT_VIEW_H_

#include <functional>
#include <memory>
#include <vector>

#include <QTimer>
#include <QWidget>

#include <MercuryModuleExt.h>
#include <ddModule.h>
#include <QMenu>

#include "no_focus_delegate.h"
#include "recent_trace_model.h"

namespace Ui
{
    class FileClientView;
}

/// @brief File-based client view
class FileClientView : public QWidget
{
    Q_OBJECT

    /// @brief Data for a custom dropdown option.
    struct CustomDropdownOption
    {
        QString                             name;            ///< The name of the option.
        bool                                is_multiselect;  ///< True if option is for multiselect
        std::function<bool(const QString&)> is_applicable;   ///< Returns true for a trace file path if the action is applicable, false otherwise.
        std::function<void(const QString&)> execute;         ///< Handler for executing the action.
        bool                                is_bold;         ///< true if the option should be displayed as bold, false otherwise.
        bool                                is_at_top;       ///< true if the option should be at the top.
    };

public:
    /// @brief Constructor
    /// @param [in] file_concept_name The terminology used to refer to a file (profile, trace, scene, etc)
    /// @param [in] extension The file extension (.rgp, .rmt, .rra, etc)
    /// @param [in] parent The parent widget
    FileClientView(const QString& file_concept_name, const QString& extension, QWidget* parent = nullptr);

    /// @brief Destructor
    virtual ~FileClientView();

public:
    /// @brief Sets the output path for this view.
    /// @param [in] path The output path for this view.
    void SetOutputPath(const QString& path);

    /// @brief Load from output directory
    /// @param [in] outdir The path to output directory
    void Load(const QString& path);

    /// @brief Sets whether or not files can be opened from the context menu.
    /// @param [in] enable_open true if context menu file opening should be enabled.
    void SetEnableOpenContextOption(bool enable_open);

    /// @brief Sets whether or not the dropdown options that edit files (rename, delete, etc.) are enabled.
    /// @param [in] edit_dropdown_options_enabled true if the edit dropdown options should be enabled, false otherwise.
    void SetEditDropdownOptionsEnabled(bool edit_dropdown_options_enabled);

    /// @brief Adds a custom option to the file dropdown.
    /// @param [in] name The name of the action.
    /// @param [in] is_applicable Returns true for a trace file path if the action is applicable, false otherwise.
    /// @param [in] execute The action to execute on each file that was selected.
    /// @param [in] is_multiselect Flag to specify this action should be available when multiple files are selected
    /// @param [in] is_bold true if the option should be bold, false otherwise.
    /// @param [in] is_at_top true if the option should be at the top.
    void AddCustomDropdownOption(const QString&                             name,
                                 const std::function<bool(const QString&)>& is_applicable,
                                 const std::function<void(const QString&)>& execute,
                                 bool                                       is_multiselect = false,
                                 bool                                       is_bold        = false,
                                 bool                                       is_at_top      = false);

private slots:

    /// @brief Handle response to double click on recent file table entry
    /// @param [in] current The selected index
    void OnRecentFileDoubleClicked(const QModelIndex& current);

    /// @brief Handle response to recent files table context menu requested
    /// @param [in] pos The position of click in table
    void OnShowRecentFilesContextMenu(const QPoint& pos);

    /// @brief Handle response to errors on recent file rename
    /// @param [in] message A human-redable error message
    void OnFileNameValidationError(const QString& message);

    /// @brief Handle response to error message timer deadline
    void OnHideErrorMessage();

private:
    /// @brief Adds custom dropdown options.
    /// @param [in] is_valid true if the current selection is valid, false otherwise.
    /// @param [in] is_multiple_selected true if multiple items are selected.
    /// @param [in] menu The menu to add the actions on.
    /// @param [in] selected_path The currently selected path.
    /// @param [out] custom_actions The map of actions to dropdown options.
    /// @param [in] is_top Only actions where is_at_top == is_top will be added.
    void AddCustomDropdownActions(bool                                                       is_valid,
                                  bool                                                       is_multiple_selected,
                                  QMenu&                                                     menu,
                                  const QString&                                             selected_path,
                                  std::unordered_map<QAction*, const CustomDropdownOption*>& custom_actions,
                                  bool                                                       is_top);

signals:
    /// @brief Signal to open the file in external tool
    /// @param [in] path The file path
    void OpenFile(const QString& path);

    /// @brief Emitted when a file is removed from disk.
    /// @param [in] path The path of the file that was removed.
    void RemovedFile(const QString& path);

    /// @brief Emitted when user selects Background Test on a supported file (rgp/rmv/rra).
    /// @param [in] path The path to the trace/profile file.
    void BackendTestRequested(const QString& path);

protected:
    std::unique_ptr<Ui::FileClientView> base_ui_;             ///< Qt ui
    std::unique_ptr<RecentTraceModel>   recent_trace_model_;  ///< Recent trace model
    std::unique_ptr<NoFocusDelegate>    no_focus_delegate_;   ///< Delegate that removes the focus border from tree items.
    std::unique_ptr<QTimer>             e_msg_timer_;         ///< Timer that hides error message
    std::vector<QWidget*>               content_widgets_;     ///< handle to content widgets

private:
    /// @brief Validates the path
    /// @param [in] path The output path
    /// @param [in] show_error True to show error
    /// @return true if valid, false otherwise
    bool ValidatePath(QString path, bool show_error = true) const;

    QString file_concept_name_;        ///< Conceptual name for a file (Profile, Capture, Trace, etc)
    QString file_concept_name_lower_;  ///< Lowercase version of conceptual file name

    std::vector<CustomDropdownOption> custom_dropdown_options_;          ///< Custom actions for the dropdown on the file list.
    bool                              enable_open_context_menu_ = true;  ///< true if the open option is enabled in the dropdown.
    bool edit_dropdown_options_enabled_ = true;  ///< true if the dropdown options that edit files (delete, rename, etc.) should be enabled, false otherwise.
};

#endif
