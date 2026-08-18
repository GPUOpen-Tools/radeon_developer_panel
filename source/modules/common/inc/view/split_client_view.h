// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  View that has capture UI on the left and recent traces on the right.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_VIEW_SPLIT_CLIENT_VIEW_H_
#define RDP_SOURCE_MODULES_COMMON_INC_VIEW_SPLIT_CLIENT_VIEW_H_

#include <memory>

#include <QWidget>
#include "model_binder.h"

#include "common/inc/model/trace_source_view_model.h"

namespace Ui
{
    class SplitClientView;
}

/// @brief View for interacting with a connected client that is split into two columns.
class SplitClientView : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] module_name The name of the module (to be displayed for the output path text).
    /// @param [in] parent The parent widget
    SplitClientView(const QString& module_name, QWidget* parent = nullptr);

    /// @brief Destructor.
    ~SplitClientView() override;

    /// @brief Sets the userdata view model for this view.
    ///
    /// This is only used to set the output path. We do it like this because the userdata view model is responsible for serializing all the settings,
    /// so it preserves it as a single point of truth. When the output path changes, the new expanded path will be propagated back to this view via the
    /// child classes view model.
    /// @param [in] userdata_view_model The userdata view model that this view should use.
    void SetUserdataViewModel(const std::shared_ptr<class UserdataViewModel>& userdata_view_model);

    /// @brief Adds a widget to the right column.
    /// @param [in] widget The widget to add to the right column.
    void AddRightColumnWidget(QWidget* widget);

    /// @brief Returns whether this view already has a widget in the content view.
    /// @param [in] widget The widget to check if it is already in the content view.
    /// @return true if the widget is in the scroll view contents, false otherwise.
    bool HasContentWidget(QWidget* widget);

    /// @brief Adds a new widget in the scroll view.
    /// @param [in] widget The widget to add.
    void AddContentWidget(QWidget* widget);

    /// @brief Adds a new widget in the scroll view at the specified index.
    /// @param [in] widget The widget to add.
    /// @param [in] index The index to add the widget at.
    void InsertContentWidget(QWidget* widget, int index);

    /// @brief Removes the content widget at the given index.
    /// @param [in] widget The widget to remove.
    void RemoveContentWidget(QWidget* widget);

    /// @brief Updates the message displayed in the status text label for unsupported state.
    /// @param [in] description The text for the status description.
    void UpdateUnsupportedDescription(const QString& description);

    /// @brief Updates the message displayed in the unsupported text label.
    /// @param [in] title The title text for the status description
    /// @param [in] description The text for the status description.
    void UpdateStatusDescription(const QString& title, const QString& description);

    /// @brief Sets whether the unsupported text should be shown or not.
    /// @param [in] show true if the text should be shown, false otherwise.
    void SetShowUnsupportedMessage(bool show);

public slots:
    /// @brief Sets the text for the connected process label.
    /// @param [in] connected_process_text The text for the connected process label.
    void SetConnectedProcessText(const QString& connected_process_text);

    /// @brief Sets the status info for client.
    /// @param [in] status The new status info.
    void SetStatus(const StatusInfo& status);

    /// @brief Called when the output path changes.
    /// @param [in] info The output path info.
    void OnOutputPathChanged(const OutputPathInfo& info);

protected:
    /// @brief Handles when the output path changes.
    /// @param [in] new_path The new output path.
    virtual void HandleOutputPathChanged(const QString& new_path) = 0;

private slots:

    /// @brief Called when the output path edit gets focus.
    void OutputEditFocusIn();

    /// @brief Called when the output path edit loses focus.
    void OutputEditFocusOut();

    /// @brief Called when the button to browse for the output path is pressed.
    void OnBrowseDumpOutputDirectory();

    /// @brief Called when the output path finishes editing.
    void OutputEditingFinished();

    /// @brief Called when the userdata view model's output path changes.
    /// @param [in] output_path The new output path.
    void OnUserdataOutputPathChanged(const QString& output_path);

protected:
    /// @brief Gets the current output path.
    /// @return The current output path.
    [[nodiscard]] QString GetCurrentOutputPath() const;

    /// @brief Gets the module name.
    /// @return The module name.
    QString GetModuleName() const;

private:
    std::unique_ptr<Ui::SplitClientView>     base_ui_;              ///< The ui for this view.
    std::shared_ptr<class UserdataViewModel> userdata_view_model_;  ///< The view model for the userdata view.

    QString module_name_;  ///< The name of the module.

    QString output_path_;  ///< The current trace output path.
    QString raw_path_;     ///< The raw output path.
};

/// @brief SplitClientView that uses a FileClientView to display files in the right column.
class SplitClientFileView : public SplitClientView
{
public:
    /// @brief Constructor
    /// @param [in] module_name The name of the module (to be displayed for the output path text).
    /// @param [in] file_concept_name The terminology used to refer to a file (profile, trace, scene, etc.)
    /// @param [in] extension The file extension (.rgp, .rmt, .rra, etc)
    /// @param [in] parent The parent widget
    SplitClientFileView(const QString& module_name, const QString& file_concept_name, const QString& extension, QWidget* parent = nullptr);

    /// @brief Destructor.
    ~SplitClientFileView() override;

protected:
    /// @brief Handles when the output path changes.
    /// @param [in] new_path The new output path.
    void HandleOutputPathChanged(const QString& new_path) override;

    /// @brief Reloads the files that are displayed in the file client view.
    void ReloadFiles();

    std::unique_ptr<class FileClientView> file_client_view_;  ///< The view that displays the recent trace files.
};

#endif
