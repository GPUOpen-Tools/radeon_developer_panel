// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP profiling module utility view class definition

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_USERDATA_VIEW_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_USERDATA_VIEW_H_

#include <memory>

#include <source_userdata.h>

#include <common/inc/utility_view.h>
#include <common/inc/view/model_binder.h>

#include "profiling_userdata_view_model.h"

namespace Ui
{
    class ProfilingCapture;
    class ProfilingSpmCounters;
    class ProfilingSqtt;
    class ProfilingAutoCapture;
    class ProfilingFrameCapture;
    class ProfilingShaderInstrumentation;
}  // namespace Ui

class ProfilingUserdataView final : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent widget if any.
    explicit ProfilingUserdataView(QWidget* parent = nullptr);

    /// @brief Destructor
    ~ProfilingUserdataView() noexcept override;

    [[nodiscard]] Ui::ProfilingCapture* GetCaptureUi() const;

private:
    /// @brief Handle setting up the initial UI
    /// This method handles setting up the initial UI, setting
    /// size policy changes or default states of widgets
    void SetupUi();

public:
    /// @brief Sets the model used for this view.
    /// @param [in] view_model The model for this view.
    void SetModel(const std::shared_ptr<ProfilingUserdataViewModel>& view_model);

private slots:
    void OnEnableInstructionTracingChanged(bool enabled) const;

    void OnEnableInstructionTracingCheckStateChanged(Qt::CheckState state) const;

    void OnEnableSpmCaptureChanged(bool enabled) const;

    void OnEnableSpmCaptureCheckStateChanged(Qt::CheckState state) const;

    void OnEnableShaderInstrumentationChanged(bool enabled) const;

    void OnEnableShaderInstrumentationCheckStateChanged(Qt::CheckState state) const;

    void OnEnableLegacyCaptureChanged(bool enabled) const;

    void OnEnableLegacyCaptureCheckStateChanged(Qt::CheckState state) const;

    /// @brief Handles when the disable capture timeout state changes.
    /// @param [in] disabled true if capture timeout is disabled, false otherwise.
    void OnDisableCaptureTimeoutChanged(bool disabled) const;

    /// @brief Handles when the disable capture timeout check box state changes.
    /// @param [in] state The new state of the check box.
    void OnDisableCaptureTimeoutCheckStateChanged(Qt::CheckState state) const;

    /// @brief Handles when the exec/pop tokens check box state changes.
    /// @param [in] state The new state of the check box.
    void OnEnableExecPopTokensCheckStateChanged(Qt::CheckState state) const;

    /// @brief Handle changing the item for the SQTT buffer size profile dropdown.
    /// @param [in] index The index that was selected in the dropdown.
    void OnSqttBufferSizeProfileDropdownChanged(int index) const;

    /// @brief Called when the SQTT buffer size changes.
    /// @param [in] profile The new SQTT buffer size profile.
    void OnSqttBufferSizeProfileChanged(devtrace::SqttBufferSizeProfiles profile) const;

    /// @brief Handle changing the item for the auto capture mode dropdown.
    /// @param [in] index The index of the auto capture mode to use.
    void OnAutoCaptureModeDropdownChanged(int index) const;

    /// @brief Called when the auto capture mode changes.
    /// @param [in] mode The new auto capture mode to use.
    void OnAutoCaptureModeChanged(devtrace::AutoCaptureMode mode) const;

    /// @brief Handle response to compute auto capture timer spin box changing.
    /// @param [in] time_ms The new time in milliseconds.
    void OnComputeAutoCaptureTimeBoxChanged(int time_ms) const;

    /// @brief Called when the compute auto capture time changes.
    /// @param [in] time_ms The new time in milliseconds.
    void OnComputeAutoCaptureTimeChanged(uint32_t time_ms) const;

    /// @brief Handle response to compute dispatch start index spin box changes.
    /// @param [in] dispatch_start The new value.
    void OnDispatchStartBoxChanged(int dispatch_start) const;

    /// @brief Called when the compute dispatch start index changes.
    /// @param [in] dispatch_start The new value.
    void OnDispatchStartChanged(int dispatch_start) const;

    /// @brief Handle response to compute dispatch count spin box changes.
    /// @param [in] dispatch_count The new value.
    void OnDispatchCountBoxChanged(int dispatch_count) const;

    /// @brief Called when the compute dispatch count changes.
    /// @param [in] dispatch_count The new value.
    void OnDispatchCountChanged(int dispatch_count) const;

    /// @brief Handles when the frame capture index spin box changes.
    /// @param [in] frame_index The new frame index to capture.
    void OnFrameCaptureIndexBoxChanged(int frame_index) const;

    /// @brief Handles when the frame capture index changes.
    /// @param [in] frame_index The new frame index to capture.
    void OnFrameCaptureIndexChanged(uint32_t frame_index) const;

    /// @brief Called when the view model fails to toggle shader instrumentation.
    void OnShaderInstrumentationFailedToSet();

    /// @brief Handles when exec/pop tokens enabled changes from model.
    /// @param [in] enabled true if exec/pop count tokens should be enable, false otherwise.
    void OnExecPopTokensChanged(bool enabled);

    /// @brief Handles when exec/pop tokens hardware support changes.
    /// @param [in] supported true if exec/pop tokens are supported on current hardware (RDNA4+), false otherwise.
    void OnExecPopTokensSupportedChanged(bool supported);

    /// @brief Handles when SPM capture hardware support changes.
    /// @param [in] supported true if SPM capture is supported on the connected device, false otherwise.
    void OnSpmCaptureSupportedChanged(bool supported) const;

    /// @brief Called when the prelaunch settings editable state changes.
    /// @param [in] enabled true if prelaunch settings should be editable, false otherwise.
    void OnPrelaunchSettingsEditableChanged(bool enabled) const;

    /// @brief Called when the ability to edit shader instrumentation changes.
    /// @param [in] enabled true if shader instrumentation checkbox should be enabled, false otherwise.
    void OnEditShaderInstrumentationEnabledChanged(bool enabled) const;

    /// @brief Called when the auto capture settings editable state changes.
    /// @param [in] enabled true if auto capture settings should be editable, false otherwise.
    void OnAutoCaptureSettingsEditableChanged(bool enabled) const;

private:
    std::shared_ptr<ProfilingUserdataViewModel>         view_model_;          ///< The model for this view.
    ModelBinder                                         model_binder_;        ///< Object used to bind to the model.
    std::unique_ptr<Ui::ProfilingCapture>               capture_ui_;          ///< Capture UI.
    std::unique_ptr<Ui::ProfilingSpmCounters>           spm_ui_;              ///< Spm counter UI.
    std::unique_ptr<Ui::ProfilingSqtt>                  sqtt_ui_;             ///< SQTT UI.
    std::unique_ptr<Ui::ProfilingAutoCapture>           auto_capture_ui_;     ///< Auto capture UI.
    std::unique_ptr<Ui::ProfilingShaderInstrumentation> instrumentation_ui_;  ///< Shader instrumentation UI.

    bool instruction_tracing_enabled_ = false;  ///< Cached instruction tracing enabled state.
    bool exec_pop_tokens_supported_   = false;  ///< Cached exec/pop tokens hardware support state.

    static constexpr uint32_t kFrameIndexMinimum    = 5;  ///< Minimum start frame index for frame-based captures
    static constexpr uint32_t kDispatchIndexMinimum = 1;  ///< Minimum start dispatch index for dispatch-based captures
};

#endif
