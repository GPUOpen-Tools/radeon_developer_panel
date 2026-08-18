// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RRA module utility view class definition

#ifndef RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_USERDATA_VIEW_H_
#define RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_USERDATA_VIEW_H_

#include <memory>

#include <QCheckBox>
#include <QLineEdit>

#include <source_userdata.h>

#include <common/inc/utility_view.h>
#include <common/inc/view/base_utility_view.h>

#include "raytracing_userdata_view_model.h"

namespace Ui
{
    class RaytracingCapture;
    class RaytracingRayHistory;
}  // namespace Ui

/// @brief Raytracing utility view
class RaytracingUserdataView final : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    RaytracingUserdataView();

    /// @brief Destructor.
    ~RaytracingUserdataView() noexcept override;

    /// @brief Sets the model for this view.
    /// @param [in] view_model The new model to use with this view.
    void SetModel(const std::shared_ptr<RaytracingUserdataViewModel>& view_model);

    /// @brief Gets the capture UI.
    /// @return The capture UI.
    [[nodiscard]] Ui::RaytracingCapture* GetCaptureUi() const;

private slots:

    /// @brief Called when the editing is finished for the ray history buffer.
    void OnRayHistoryBufferEditingFinished();

    /// @brief Called when the ray history buffer size changes.
    /// @param [in] buffer_size A string representation of the buffer size.
    void OnRayHistoryBufferSizedChanged(const std::string& buffer_size);

    /// @brief Called when the ray history buffer size dropdown selection changes.
    /// @param [in] index The new selection index of the dropdown.
    void OnRayHistoryBufferSizeIndexSelectionChanged(int index);

    /// @brief Called when the ray history buffer size dropdown index changes.
    /// @param [in] index The new selection index of the dropdown.
    void OnRayHistoryBufferSizeIndexChanged(int index);

    /// @brief Called when marker capture enabled changes from view model.
    /// @param [in] enabled true if marker capture is enabled.
    void OnEnableMarkerCaptureChanged(bool enabled);

    /// @brief Called when the marker begin string changes from view model.
    /// @param [in] marker_string The new marker begin string.
    void OnMarkerBeginStringChanged(const QString& marker_string);

    /// @brief Called when the marker end string changes from view model.
    /// @param [in] marker_string The new marker end string.
    void OnMarkerEndStringChanged(const QString& marker_string);

    /// @brief Called when marker begin line edit loses focus.
    void OnMarkerBeginEditingFinished();

    /// @brief Called when marker end line edit loses focus.
    void OnMarkerEndEditingFinished();

    /// @brief Called when the marker capture support status changes.
    /// @param [in] supported true if marker-based capture is supported by the driver.
    void OnMarkerCaptureSupportedChanged(bool supported);

private:
    void SetupUi();

    std::unique_ptr<Ui::RaytracingCapture>       capture_ui_;                      ///< The capture UI.
    std::unique_ptr<Ui::RaytracingRayHistory>    ray_history_ui_;                  ///< The ray history UI.
    std::shared_ptr<RaytracingUserdataViewModel> view_model_;                      ///< View model.
    ModelBinder                                  model_binder_;                    ///< Utility object used to bind to a model.
    QCheckBox*                                   enable_marker_capture_checkbox_;  ///< Checkbox for enabling marker capture.
    QLineEdit*                                   marker_begin_edit_;               ///< Line edit for marker begin string.
    QLineEdit*                                   marker_end_edit_;                 ///< Line edit for marker end string.
    QWidget*                                     marker_capture_pane_;             ///< The marker capture collapsible pane.
};

#endif
