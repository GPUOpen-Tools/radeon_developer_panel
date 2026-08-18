// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Crash analysis module client view class definition.

#ifndef RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_VIEW_H_
#define RDP_SOURCE_MODULES_CRASHANALYSIS_SRC_GUI_CRASH_ANALYSIS_VIEW_H_

#include <memory>

#include <QTimer>

#include <common/inc/view/model_binder.h>
#include <common/inc/view/split_client_view.h>
#include "crash_analysis_userdata_view_model.h"
#include "crash_analysis_view_model.h"

// ReSharper disable once CppInconsistentNaming
namespace Ui
{
    class CrashAnalysisAuxWidget;
}

/// @brief Crash analysis client view.
class CrashAnalysisView final : public SplitClientFileView
{
public:
    /// @brief Constructor.
    /// @param [in] utility_view The utility view that manages settings.
    /// @param [in] view_model The view model for the crash analysis view.
    /// @param [in] parent The parent widget for this widget.
    explicit CrashAnalysisView(QWidget* utility_view, const std::shared_ptr<CrashAnalysisViewModel>& view_model, QWidget* parent = nullptr);

    /// @brief Destructor.
    ~CrashAnalysisView() override;

    /// @brief Sets the userdata view model.
    /// @param [in] userdata_view_model The userdata view model.
    void SetUserdataViewModel(const std::shared_ptr<CrashAnalysisUserdataViewModel>& userdata_view_model);

private Q_SLOTS:
    /// @brief Called when the button to show the cached error message is pressed.
    void ShowSummaryError() const;

    /// @brief Called when the accessory info changes.
    /// @param [in] info The new accessory info.
    void OnAccessoryInfoChanged(const CrashAnalysisAccessoryInfo& info);

    /// @brief Called when APU hardware detection status changes.
    /// @param [in] is_apu true if the current hardware is an APU.
    void OnIsCurrentHardwareApuChanged(bool is_apu);

    /// @brief Called when hardware crash analysis support status changes.
    /// @param [in] supported true if hardware crash analysis is supported.
    void OnHardwareCrashAnalysisSupportedChanged(bool supported);

    /// @brief Called when trying to open a file and the app executable is missing.
    /// @param [in] path The path where the app executable was expected.
    void OnApplicationExecutableMissing(const QString& path);

    /// @brief Called when trying to open a file and the text editor is missing.
    /// @param [in] path The path where the text editor was expected.
    void OnTextEditorMissing(const QString& path);

    /// @brief Called when a trace is ended (failure or success).
    void OnTraceEnded();

    /// @brief Called when current connections change.
    /// @param [in] connections The current connections.
    void OnCurrentConnectionsChanged(std::unordered_map<DDConnectionId, devtrace::Api> connections);

protected:
    /// @brief Shows the aux widget.
    void ShowAuxWidget();

private:
    /// @brief Sets the model used by this view.
    void SetupConnections();

    /// @brief Opens the file with the provided path and extension in a text editor.
    /// @param [in] path The path of the file to open.
    /// @param [in] extension The extension of the file to open (will change the path).
    void OpenFileInTextEditor(const QString& path, const QString& extension) const;

    /// @brief Queues the generation of an RGD summary.
    /// @param [in] path The path of the RGD file to generate the summary for.
    /// @param [in] generate_text true if the text summary should be generated, false otherwise.
    /// @param [in] generate_json true if the json summary should be generated, false otherwise.
    void QueueSummaryGeneration(const QString& path, bool generate_text, bool generate_json) const;

    /// @brief Checks if a file exists with the same name as the given path with the given extension.
    /// @param [in] path The path of the file to check.
    /// @param [in] extension The extension to see if a file exists with the same name.
    /// @return true if a file exists at the same path with the given extension, false otherwise.
    static bool FileExistsWithOtherExtension(const QString& path, const QString& extension);

    ModelBinder                                   model_binder_;                  ///< Utility object used to bind to a model.
    std::weak_ptr<CrashAnalysisViewModel>         view_model_;                    ///< The view model as a crash analysis model.
    std::weak_ptr<CrashAnalysisUserdataViewModel> userdata_view_model_;           ///< The userdata view model.
    std::unique_ptr<Ui::CrashAnalysisAuxWidget>   aux_widget_ui_;                 ///< The auxiliary widget UI.
    QWidget*                                      aux_widget_;                    ///< The auxiliary widget.
    QTimer                                        no_crash_detected_hide_timer_;  ///< Timer to auto-hide the "no crash detected" message.
};

#endif
