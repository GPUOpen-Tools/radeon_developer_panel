// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Crash analysis module utility view model class definition.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_USERDATA_VIEW_MODEL_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_USERDATA_VIEW_MODEL_H_

#include <memory>

#include <source_userdata_mapper.h>

#include <common/inc/model/utility/base_userdata_view_model.h>

#include "crash_analysis_prelaunch_settings_helper.h"

/// @brief View model for the crash analysis utility view.
class CrashAnalysisUserdataViewModel final : public BaseUserdataViewModel<devtrace::RgdUserdataMapper>
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] mapper Object used to map userdata to and from JSON.
    /// @param [in] rgd_trace_source The RGD trace source.
    /// @param [in] prelaunch_helper The prelaunch settings helper.
    /// @param [in] output_path_parent_folder The name of the default output path in the user's documents folder.
    /// @param [in] apply_fn The function used to apply changes.
    explicit CrashAnalysisUserdataViewModel(const std::shared_ptr<devtrace::RgdUserdataMapper>&          mapper,
                                            const std::shared_ptr<devtrace::RgdTraceSource>&             rgd_trace_source,
                                            const std::shared_ptr<CrashAnalysisPrelaunchSettingsHelper>& prelaunch_helper,
                                            const std::string&                                           output_path_parent_folder,
                                            const std::function<void(const std::string&)>&               apply_fn);

    bool ReceiveUserData(const std::string& data) override;

public slots:

    /// @brief Handles when whether or not the text summary should be generated changes.
    /// @param [in] generate_text_summary true if a text report should be generated, false otherwise.
    void HandleGenerateTextSummaryChanged(int generate_text_summary);

    /// @brief Handles when whether or not the JSON summary should be generated changes.
    /// @param [in] generate_json_summary true if a JSON report should be generated, false otherwise.
    void HandleGenerateJsonSummaryChanged(int generate_json_summary);

    /// @brief Handles when whether or not summaries should have source information for markers changes.
    /// @param [in] show_marker_source true if summaries should have source information for markers.
    void HandleShowMarkerSourceChanged(int show_marker_source);

    /// @brief Handles when whether or not summaries should expand the marker tree changes.
    /// @param [in] expand_markers true if summaries should expand the marker tree.
    void HandleExpandMarkersChanged(int expand_markers);

    /// @brief Handles when the enable enhanced crash checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandleEnableEnhancedCrashChanged(int state);

    /// @brief Handles when the disable serialize memory ops checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandleDisableSerializeMemOpsChanged(int state);

    /// @brief Handles when the disable serialize ALU ops checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandleDisableSerializeAluOpsChanged(int state);

    /// @brief Handles when the collect SGPRs checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandleCollectSgprsChanged(int state);

    /// @brief Handles when the collect VGPRs checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandleCollectVgprsChanged(int state);

    /// @brief Handles when the PDB search paths change.
    /// @param [in] paths The new PDB search paths as a semicolon-delimited string.
    void HandlePdbSearchPathsChanged(const QString& paths);

    /// @brief Handles when the PDB include subfolders checkbox changes.
    /// @param [in] state The new checkbox state.
    void HandlePdbIncludeSubfoldersChanged(int state);

    /// @brief Handles when the application connected state changes.
    /// @param [in] connected true if an application is connected, false otherwise.
    void HandleApplicationConnectedChanged(bool connected);

signals:

    /// @brief Emitted when whether or not the text summary should be generated changes.
    /// @param [in] generate_text_summary true if a text report should be generated, false otherwise.
    void GenerateTextSummaryChanged(bool generate_text_summary);

    /// @brief Emitted when whether or not the JSON summary should be generated changes.
    /// @param [in] generate_json_summary true if a JSON report should be generated, false otherwise.
    void GenerateJsonSummaryChanged(bool generate_json_summary);

    /// @brief Emitted when whether or not summaries should have source information for markers changes.
    /// @param [in] show_marker_source true if summaries should have source information for markers.
    void ShowMarkerSourceChanged(bool show_marker_source);

    /// @brief Emitted when whether or not summaries should expand the marker tree changes.
    /// @param [in] expand_markers true if summaries should expand the marker tree.
    void ExpandMarkersChanged(bool expand_markers);

    /// @brief Emitted when hardware crash analysis support status changes.
    /// @param [in] supported true if hardware crash analysis is supported, false otherwise.
    void HardwareCrashAnalysisSupportedChanged(bool supported);

    /// @brief Emitted when wave SGPR/VGPR capture support status changes.
    /// @param [in] supported true if the installed driver supports GPR capture, false otherwise.
    void GprCaptureSupportedChanged(bool supported);

    /// @brief Emitted when the enable enhanced crash setting changes.
    /// @param [in] enabled true if enhanced crash is enabled.
    void EnableEnhancedCrashChanged(bool enabled);

    /// @brief Emitted when the disable serialize memory ops setting changes.
    /// @param [in] disabled true if serialize memory ops is disabled.
    void DisableSerializeMemOpsChanged(bool disabled);

    /// @brief Emitted when the disable serialize ALU ops setting changes.
    /// @param [in] disabled true if serialize ALU ops is disabled.
    void DisableSerializeAluOpsChanged(bool disabled);

    /// @brief Emitted when the collect SGPRs setting changes.
    /// @param [in] collect true if SGPRs should be collected.
    void CollectSgprsChanged(bool collect);

    /// @brief Emitted when the collect VGPRs setting changes.
    /// @param [in] collect true if VGPRs should be collected.
    void CollectVgprsChanged(bool collect);

    /// @brief Emitted when the PDB search paths change.
    /// @param [in] paths The PDB search paths as a semicolon-delimited string.
    void PdbSearchPathsChanged(const QString& paths);

    /// @brief Emitted when the PDB include subfolders setting changes.
    /// @param [in] include_subfolders true if PDB search should include subfolders.
    void PdbIncludeSubfoldersChanged(bool include_subfolders);

    /// @brief Emitted when the application connected state changes.
    /// @param [in] connected true if an application is connected, false otherwise.
    void ApplicationConnectedChanged(bool connected);

protected:
    /// @brief Initialize the default values.
    ///
    /// The base class implementation will handle the trace output path, so the superclass method should be called
    /// if this is overridden.
    /// @param [out] userdata The userdata struct to fill with defaults.
    void InitializeDefaults(UserdataType& userdata) override;

    /// @brief Called when the entire userdata changes.
    ///
    /// This should emit on updated signals for all of the properties on the model.
    /// The base class implementation will handle the trace output path.
    /// @param [in] userdata The new userdata.
    void OnUserdataChanged(const UserdataType& userdata) override;

public:
    /// @brief Sets the supported flag for hardware crash analysis
    /// @param supported True if supported, false otherwise.
    void SetHardwareCrashAnalysisSupported(bool supported);

    /// @brief Sets the supported flag for wave SGPR/VGPR capture.
    /// @param supported True if the installed driver supports GPR capture, false otherwise.
    void SetGprCaptureSupported(bool supported);

    /// @brief Should be called when a view binds to the model.
    ///
    /// Re-emits support-capability flags so late-bound views receive the correct state even if the
    /// driver's support event fired before SetModel() was called.
    void OnBind() override;

private:
    /// @brief Converts a vector of strings to a semicolon-delimited QString.
    /// @param [in] paths The vector of paths.
    /// @return The semicolon-delimited string.
    static QString PathsToString(const std::vector<std::string>& paths);

    /// @brief Converts a semicolon-delimited QString to a vector of strings.
    /// @param [in] str The semicolon-delimited string.
    /// @return The vector of paths.
    static std::vector<std::string> StringToPaths(const QString& str);

    std::weak_ptr<devtrace::RgdTraceSource>               rgd_trace_source_;                          ///< The RGD trace source.
    std::shared_ptr<CrashAnalysisPrelaunchSettingsHelper> prelaunch_helper_;                          ///< Object used for prelaunch settings.
    bool                                                  hardware_crash_analysis_supported_{false};  ///< Cached hardware crash analysis support flag.
    bool                                                  gpr_capture_supported_{false};              ///< Cached SGPR/VGPR capture support flag.
    bool                                                  support_event_received_{false};             ///< True once a driver support event has been received.
};

#endif
