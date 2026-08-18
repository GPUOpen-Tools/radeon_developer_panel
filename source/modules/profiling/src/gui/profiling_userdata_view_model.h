// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module utility view model class definition.

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_UTILITY_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_UTILITY_VIEW_MODEL_H_

#include <memory>

#include <rgp_trace_source.h>
#include <source_userdata.h>
#include <source_userdata_mapper.h>

#include <common/inc/delay_store.h>
#include <common/inc/global_shortcut.h>
#include <common/inc/model/utility/base_userdata_view_model.h>

/// @brief View model for the profiling userdata view.
class ProfilingUserdataViewModel final : public BaseUserdataViewModel<devtrace::RgpUserdataMapper>
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] mapper Object used to map userdata to and from JSON.
    /// @param [in] rgp_trace_source The RGP trace source to use for profiling.
    /// @param [in] prelaunch_helper Helper for prelaunch settings.
    /// @param [in] output_path_parent_folder The name of the default output path in the user's documents folder.
    /// @param [in] apply_fn The function used to apply changes.
    explicit ProfilingUserdataViewModel(const std::shared_ptr<devtrace::RgpUserdataMapper>&            mapper,
                                        const std::shared_ptr<devtrace::RgpTraceSource>&               rgp_trace_source,
                                        const std::shared_ptr<class ProfilingPrelaunchSettingsHelper>& prelaunch_helper,
                                        const std::string&                                             output_path_parent_folder,
                                        const std::function<void(const std::string&)>&                 apply_fn);

    bool ReceiveUserData(const std::string& data) override;

    devtrace::AutoCaptureMode GetAutoCaptureMode() const;

    static const GlobalShortcut& GetDefaultShortcut();

    DelayInfo GetDelayInfo() const;

protected:
    /// @brief Initialize the default values.
    ///
    /// The base class implementation will handle the trace output path, so the superclass method should be called
    /// if this is overridden.
    /// @param [out] userdata The userdata struct to fill with defaults.
    void InitializeDefaults(UserdataType& userdata) override;

    /// @brief When LoadSerializedData is called, this function provides an opportunity to modify the data if it is invalid.
    void ValidateData(UserdataType& userdata) override;

    /// @brief Called when the entire userdata changes.
    ///
    /// This should emit on updated signals for all the properties on the model.
    /// The base class implementation will handle the trace output path.
    /// @param [in] userdata The new userdata.
    void OnUserdataChanged(const UserdataType& userdata) override;

public slots:

    void HandleCaptureDelayChanged(uint32_t delay_ms);

    void HandleShouldDelayCaptureChanged(bool should_delay);

    void HandleCaptureShortcutChanged(const GlobalShortcut& shortcut);

    void HandleInstructionTracingEnabledChanged(bool enabled);

    void HandleSpmCaptureEnabledChanged(bool enabled);

    void HandleLegacyCaptureEnabledChanged(bool enabled);

    /// @brief Handles when the disable capture timeout state changes.
    /// @param [in] disabled true if capture timeout should be disabled, false otherwise.
    void HandleDisableCaptureTimeoutChanged(bool disabled);

    /// @brief Handles when the exec pop tokens enabled state changes.
    /// @param [in] enabled true if exec pop tokens should be enabled, false otherwise.
    void HandleExecPopTokensEnabledChanged(bool enabled);

    /// @brief Handles when the SQTT profile changes.
    /// @param [in] profile The new SQTT profile to use.
    void HandleSqttProfileIndexChanged(devtrace::SqttBufferSizeProfiles profile);

    /// @brief Handles when the auto capture mode changes.
    /// @param [in] mode The new auto capture mode to use.
    void HandleAutoCaptureModeChanged(devtrace::AutoCaptureMode mode);

    /// @brief Handles when the compute auto capture time changes.
    /// @param [in] time_ms The new compute auto capture time to use.
    void HandleComputeAutoCaptureTimeChanged(uint32_t time_ms);

    /// @brief Handles when the compute dispatch start index changes.
    /// @param [in] dispatch_start The new compute dispatch start index to use.
    void HandleDispatchStartChanged(uint32_t dispatch_start);

    /// @brief Handles when the compute dispatch count changes.
    /// @param [in] dispatch_count The new compute dispatch count to use.
    void HandleDispatchCountChanged(uint32_t dispatch_count);

    /// @brief Handles when the draw count changes.
    /// @param [in] draw_count The new draw count to use.
    void HandleDrawCountChanged(uint32_t draw_count);

    /// @brief Handles when the frame capture index changes.
    /// @param [in] frame_index The new frame index to capture.
    void HandleFrameCaptureIndexChanged(uint32_t frame_index);

    void HandleShaderInstrumentationEnabledChanged(bool enabled);

    /// @brief Handles when trace source support changes.
    /// @param [in] args The support event args containing feature support flags.
    void HandleTraceSupportChanged(const devtrace::RgpTraceSourceSupportEventArgs& args);

    /// @brief Handles when prelaunch settings editable state changes.
    /// @param [in] editable true if prelaunch settings should be editable, false otherwise.
    void HandlePrelaunchSettingsEditableChanged(bool editable);

signals:
    void CaptureShortcutChanged(GlobalShortcut shortcut);
    void CaptureDelayChanged(uint32_t delay_ms);
    void ShouldDelayCaptureChanged(bool should_delay);

    void DefaultCaptureModesChanged(devtrace::DefaultCaptureMode mode);

    /// @brief Emitted when the instruction tracing enabled state changes.
    void InstructionTracingEnabledChanged(bool enabled);

    void SpmCaptureEnabledChanged(bool enabled);

    void ShaderInstrumentationEnabledChanged(bool enabled);

    void LegacyCaptureEnabledChanged(bool enabled);

    /// @brief Emitted when the disable capture timeout state changes.
    /// @param [in] disabled true if capture timeout is disabled, false otherwise.
    void DisableCaptureTimeoutChanged(bool disabled);

    /// @brief Emitted when the exec pop tokens enabled state changes.
    /// @param [in] enabled true if exec pop tokens are enabled, false otherwise.
    void ExecPopTokensEnabledChanged(bool enabled);

    /// @brief Emitted when the SQTT profile changes.
    /// @param [in] profile The new SQTT profile to use.
    void SqttProfileIndexChanged(devtrace::SqttBufferSizeProfiles profile);

    /// @brief Emitted when the auto capture mode changes.
    /// @param [in] mode The new auto capture mode to use.
    void AutoCaptureModeChanged(devtrace::AutoCaptureMode mode);

    /// @brief Emitted when the compute auto capture time changes.
    /// @param [in] time_ms The new compute auto capture time to use.
    void ComputeAutoCaptureTimeChanged(uint32_t time_ms);

    /// @brief Emitted when the compute dispatch start index changes.
    /// @param [in] dispatch_start The new compute dispatch start index to use.
    void DispatchStartChanged(uint32_t dispatch_start);

    /// @brief Emitted when the compute dispatch count changes.
    /// @param [in] dispatch_count The new compute dispatch count to use.
    void DispatchCountChanged(uint32_t dispatch_count);

    /// @brief Emitted when the draw count changes.
    /// @param [in] draw_count The new draw count to use.
    void DrawCountChanged(uint32_t draw_count);

    /// @brief Emitted when the frame capture index changes.
    /// @param [in] frame_index The new frame index to capture.
    void FrameCaptureIndexChanged(uint32_t frame_index);

    /// @brief Emitted when exec/pop tokens hardware support changes.
    /// @param [in] supported true if exec/pop tokens are supported on current hardware (RDNA4+), false otherwise.
    void ExecPopTokensSupportedChanged(bool supported);

    /// @brief Emitted when SPM capture hardware support changes.
    /// @param [in] supported true if SPM capture is supported on the connected device, false otherwise.
    void SpmCaptureSupportedChanged(bool supported);

    /// @brief Emitted when the shader instrumentation couldn't be set.
    void ShaderInstrumentationFailedToSet();

    /// @brief Emitted when the prelaunch settings editable state changes.
    /// @param [in] editable true if prelaunch settings should be editable, false otherwise.
    void PrelaunchSettingsEditableChanged(bool editable);

    /// @brief Emitted when shader instrumentation universal support changes.
    /// @param [in] supported true if shader instrumentation is supported universally (all APIs), false if DX12 only.
    void ShaderInstrumentationSupportedUniversallyChanged(bool supported);

    /// @brief Emitted when the ability to edit shader instrumentation changes.
    /// @param [in] enabled true if shader instrumentation checkbox should be enabled, false otherwise.
    void EditShaderInstrumentationEnabledChanged(bool enabled);

    /// @brief Emitted when the auto capture settings editable state changes.
    /// @param [in] editable true if auto capture settings should be editable, false otherwise.
    /// This is true when prelaunch settings are editable OR auto-capture mode is None.
    void AutoCaptureSettingsEditableChanged(bool editable);

public:
    /// @brief Updates the default capture mode.
    /// @param [in] capture_mode The default capture mode.
    void UpdateDefaultCaptureMode(const devtrace::DefaultCaptureMode& capture_mode);

private:
    std::weak_ptr<devtrace::RgpTraceSource>           rgp_trace_source_;              ///< Trace source.
    std::shared_ptr<ProfilingPrelaunchSettingsHelper> prelaunch_helper_;              ///< Object used for prelaunch settings.
    devtrace::AutoCaptureMode                         auto_capture_mode_;             ///< the auto capture mode selected by user
    bool                                              should_delay_capture_ = false;  ///< true if the capture should be delayed, false otherwise.
    uint32_t                                          capture_delay_        = 100;    ///< The capture delay in milliseconds.

    bool prelaunch_settings_editable_                  = true;   ///< true if prelaunch settings should be editable.
    bool shader_instrumentation_supported_universally_ = true;   ///< true if shader instrumentation is supported universally.
    bool exec_pop_tokens_supported_                    = false;  ///< true if exec/pop tokens are supported on current hardware (RDNA4+).
    bool spm_capture_supported_                        = true;   ///< true if SPM capture is supported on the connected device.
};

#endif
