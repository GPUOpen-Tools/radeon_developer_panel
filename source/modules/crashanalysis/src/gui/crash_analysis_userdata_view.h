// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Class analysis utility view declaration.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_USERDATA_VIEW_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_USERDATA_VIEW_H_

#include <common/inc/view/base_utility_view.h>

#include "crash_analysis_userdata_view_model.h"

#include <qt_common/custom_widgets/include_directories_view.h>

// ReSharper disable once CppInconsistentNaming
namespace Ui
{
    class CrashAnalysisAutoSummaryGeneration;
    class CrashAnalysisAdvanced;
    class CrashAnalysisEnhancedCrash;
    class CrashAnalysisSymbols;
}  // namespace Ui

/// @brief Definition for the crash analysis utility view.
class CrashAnalysisUserdataView final : public QWidget
{
    Q_OBJECT
public:
    /// @brief Constructor.
    CrashAnalysisUserdataView();

    /// @brief Destructor.
    ~CrashAnalysisUserdataView() override;

    /// @brief Sets the model for this view.
    /// @param [in] view_model The new model to use with this view.
    void SetModel(const std::shared_ptr<CrashAnalysisUserdataViewModel>& view_model);

private slots:

    /// @brief Called when the PDB search path dialog is closed
    /// @param [in] paths The list of paths set in the search dialog
    void OnPdbSearchDialogConfirmed(const QStringList& paths) const;

    /// @brief Called when whether or not the text summary should be generated changes.
    /// @param [in] generate_text_summary true if a text report should be generated, false otherwise.
    void OnGenerateTextSummaryChanged(bool generate_text_summary) const;

    /// @brief Called when whether or not the JSON summary should be generated changes.
    /// @param [in] generate_json_summary true if a JSON report should be generated, false otherwise.
    void OnGenerateJsonSummaryChanged(bool generate_json_summary) const;

    /// @brief Called when whether or not summaries should have source information for markers changes.
    /// @param [in] show_marker_source true if summaries should have source information for markers.
    void OnShowMarkerSourceChanged(bool show_marker_source) const;

    /// @brief Called when whether or not summaries should expand the marker tree changes.
    /// @param [in] expand_markers true if summaries should expand the marker tree.
    void OnExpandMarkersChanged(bool expand_markers) const;

    /// @brief Called when the button to edit PDBs is pressed.
    void EditPdbPaths() const;

    /// @brief Called when hardware crash analysis support status changes.
    /// @param [in] supported true if hardware crash analysis is supported, false otherwise.
    void OnHardwareCrashAnalysisSupportedChanged(bool supported);

    /// @brief Called when wave SGPR/VGPR capture support status changes.
    /// @param [in] supported true if the installed driver supports GPR capture, false otherwise.
    void OnGprCaptureSupportedChanged(bool supported);

    /// @brief Called when the enable enhanced crash setting changes.
    /// @param [in] enabled true if enhanced crash is enabled.
    void OnEnableEnhancedCrashChanged(bool enabled) const;

    /// @brief Called when the disable serialize memory ops setting changes.
    /// @param [in] disabled true if serialize memory ops is disabled.
    void OnDisableSerializeMemOpsChanged(bool disabled) const;

    /// @brief Called when the disable serialize ALU ops setting changes.
    /// @param [in] disabled true if serialize ALU ops is disabled.
    void OnDisableSerializeAluOpsChanged(bool disabled) const;

    /// @brief Called when the collect SGPRs setting changes.
    /// @param [in] collect true if SGPRs should be collected.
    void OnCollectSgprsChanged(bool collect) const;

    /// @brief Called when the collect VGPRs setting changes.
    /// @param [in] collect true if VGPRs should be collected.
    void OnCollectVgprsChanged(bool collect) const;

    /// @brief Called when the PDB search paths change.
    /// @param [in] paths The PDB search paths as a semicolon-delimited string.
    void OnPdbSearchPathsChanged(const QString& paths) const;

    /// @brief Called when the PDB include subfolders setting changes.
    /// @param [in] include_subfolders true if PDB search should include subfolders.
    void OnPdbIncludeSubfoldersChanged(bool include_subfolders) const;

    /// @brief Called when the application connected state changes.
    /// @param [in] connected true if an application is connected, false otherwise.
    void OnApplicationConnectedChanged(bool connected);

private:
    /// @brief Updates the enabled state of the enhanced crash sub-options.
    ///
    /// Sub-options are enabled only if hardware crash analysis is supported AND enabled AND no application is connected.
    void UpdateEnhancedCrashSubOptionsEnabled() const;

    std::unique_ptr<Ui::CrashAnalysisAutoSummaryGeneration> summary_ui_;                               ///< UI for automatic summary generation.
    std::unique_ptr<Ui::CrashAnalysisAdvanced>              advanced_ui_;                              ///< UI for advanced options.
    std::unique_ptr<Ui::CrashAnalysisEnhancedCrash>         enhanced_ui_;                              ///< UI for enhanced crash analysis.
    std::unique_ptr<Ui::CrashAnalysisSymbols>               symbol_ui_;                                ///< UI for symbols crash analysis.
    std::shared_ptr<CrashAnalysisUserdataViewModel>         view_model_;                               ///< View model.
    ModelBinder                                             model_binder_;                             ///< Utility object used to bind to a model.
    IncludeDirectoriesView*                                 pdb_path_dialog_;                          ///< Dialog for specifying PDB debug file paths.
    bool                                                    hardware_crash_analysis_supported_{true};  ///< True if hardware crash analysis is supported.
    bool                                                    gpr_capture_supported_{true};   ///< True if the installed driver supports SGPR/VGPR capture.
    bool                                                    application_connected_{false};  ///< True if an application is connected.
    QString                                                 sgpr_info_default_tooltip_;  ///< The default (driver-supported) tooltip for the SGPR info button.
    QString                                                 vgpr_info_default_tooltip_;  ///< The default (driver-supported) tooltip for the VGPR info button.
};

#endif
