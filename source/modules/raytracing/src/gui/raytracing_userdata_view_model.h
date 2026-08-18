// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Raytracing module utility view model class definition.

#ifndef RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_UTILITY_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_RAYTRACING_SRC_GUI_RAYTRACING_UTILITY_VIEW_MODEL_H_

#include <memory>

#include <rra_trace_source.h>
#include <source_userdata_mapper.h>

#include <common/inc/delay_store.h>
#include <common/inc/global_shortcut.h>
#include <common/inc/model/utility/base_userdata_view_model.h>

/// @brief View model for the raytracing utility view.
class RaytracingUserdataViewModel final : public BaseUserdataViewModel<devtrace::RraUserdataMapper>
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] mapper Object used to map userdata to and from JSON.
    /// @param [in] rra_trace_source The RRA trace source.
    /// @param [in] prelaunch_helper Helper for prelaunch settings.
    /// @param [in] output_path_parent_folder The name of the default output path in the user's documents folder.
    /// @param [in] apply_fn The function used to apply changes.
    RaytracingUserdataViewModel(const std::shared_ptr<devtrace::RraUserdataMapper>& mapper,
                                const std::shared_ptr<devtrace::RraTraceSource>&    rra_trace_source,
                                const std::shared_ptr<PrelaunchSettingsHelper>&     prelaunch_helper,
                                const std::string&                                  output_path_parent_folder,
                                const std::function<void(const std::string&)>&      apply_fn);

    bool ReceiveUserData(const std::string& data) override;

    /// @brief Gets whether ray history is enabled.
    /// @return true if ray history is enabled, false otherwise.
    [[nodiscard]] bool IsRayHistoryEnabled() const;

    static const GlobalShortcut& GetDefaultShortcut();

    DelayInfo GetDelayInfo() const;

protected:
    void ValidateData(UserdataType& userdata) override;
    void InitializeDefaults(UserdataType& userdata) override;
    void OnUserdataChanged(const UserdataType& userdata) override;

public slots:

    void HandleEnableRayHistoryChanged(Qt::CheckState state);

    void HandleCaptureShortcutChanged(const GlobalShortcut& shortcut);

    void HandleCaptureDelayChanged(uint32_t delay_ms);

    void HandleShouldDelayCaptureChanged(bool should_delay);

    /// @brief Handles when the ray history buffer size changes.
    /// @param [in] buffer_size The size of the ray history buffer in bytes.
    void HandleRayHistoryBufferSizeChanged(const std::string& buffer_size);

    /// @brief Handles when the ray history buffer size index changes.
    /// @param [in] index The selection index of the ray history buffer size dropdown.
    void HandleRayHistoryBufferSizeIndexChanged(uint32_t index);

    /// @brief Handles when the enable marker capture checkbox changes.
    /// @param [in] state The new check state.
    void HandleEnableMarkerCaptureChanged(Qt::CheckState state);

    /// @brief Handles when the trace source support status changes.
    /// @param [in] args The support event arguments.
    void HandleTraceSupportChanged(const devtrace::RraTraceSourceSupportEventArgs& args);

    /// @brief Handles when the marker begin string changes.
    /// @param [in] marker_string The new marker begin string.
    void HandleMarkerBeginStringChanged(const QString& marker_string);

    /// @brief Handles when the marker end string changes.
    /// @param [in] marker_string The new marker end string.
    void HandleMarkerEndStringChanged(const QString& marker_string);

signals:

    void EnableRayHistoryChanged(bool enable_ray_history);

    void CaptureShortcutChanged(GlobalShortcut shortcut);

    void CaptureDelayChanged(uint32_t delay_ms);

    void ShouldDelayCaptureChanged(bool should_delay);

    /// @brief Emitted when the ray history buffer size changes.
    /// @param [in] buffer_size The size of the ray history buffer in bytes.
    void RayHistoryBufferSizeChanged(const std::string& buffer_size);

    /// @brief Emitted when the ray history buffer size index changes.
    /// @param [in] index The selection index for ray history buffer size dropdown.
    void RayHistoryBufferSizeIndexChanged(uint32_t index);

    /// @brief Emitted when the enable marker capture setting changes.
    /// @param [in] enabled true if marker capture is enabled.
    void EnableMarkerCaptureChanged(bool enabled);

    /// @brief Emitted when the marker begin string changes.
    /// @param [in] marker_string The new marker begin string.
    void MarkerBeginStringChanged(const QString& marker_string);

    /// @brief Emitted when the marker end string changes.
    /// @param [in] marker_string The new marker end string.
    void MarkerEndStringChanged(const QString& marker_string);

    /// @brief Emitted when the marker capture support status changes.
    /// @param [in] supported true if marker-based capture is supported by the driver.
    void MarkerCaptureSupportedChanged(bool supported);

private:
    std::weak_ptr<devtrace::RraTraceSource> rra_trace_source_;              ///< Trace source.
    bool                                    enable_ray_history_;            ///< true if ray history is enabled, false otherwise.
    bool                                    should_delay_capture_ = false;  ///< true if the capture should be delayed, false otherwise.
    uint32_t                                capture_delay_        = 100;    ///< The capture delay in milliseconds.
};

#endif
