// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  A widget that shows the progress of a trace being captured.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_CAPTURE_PROGRESS_WIDGET_H_
#define RDP_SOURCE_MODULES_COMMON_INC_CAPTURE_PROGRESS_WIDGET_H_

#include <memory>

#include <QWidget>

#include "common/inc/model/trace_source_view_model.h"

namespace Ui
{
    class CaptureProgressWidget;
}

/// @brief A panel that shows the progress of a trace being captured.
class CaptureProgressWidget : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent widget
    explicit CaptureProgressWidget(QWidget* parent = nullptr);

    /// @brief Destructor
    ~CaptureProgressWidget() override;

    /// @brief Reset the progress
    void Reset();

    /// @brief Sets whether or not the cancel button is shown.
    /// @param [in] hidden true if the button should be shown, false otherwise.
    void SetCancelButtonShown(bool hidden);

public slots:
    /// @brief Updates the progress bar
    /// @param [in] progress_info The progress information.
    void UpdateProgress(ProgressInfo progress_info);

private:
    /// @brief Sets the text on the progress label.
    /// @param [in] text The new text for the progress label.
    void SetProgressText(const QString& text);

private slots:
    /// @brief Called when the cancel button is pressed.
    void OnCancelTraceClicked();

signals:
    /// @brief Signal trace was cancelled
    void TraceCancelled();

private:
    std::unique_ptr<Ui::CaptureProgressWidget> ui_;  ///< Qt ui
};

#endif
