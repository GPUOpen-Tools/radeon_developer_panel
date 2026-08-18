// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Profiling module view class definition

#ifndef RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_VIEW_H_
#define RDP_SOURCE_MODULES_PROFILING_SRC_GUI_PROFILING_VIEW_H_

#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include <QCheckBox>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStringListModel>
#include <QThread>
#include <QValidator>

#include <MercuryModuleExt.h>
#include <ddModule.h>

#include <common/inc/capture_progress_widget.h>
#include <common/inc/global_shortcut_manager.h>
#include <common/inc/view/split_client_view.h>

#include "common/inc/model/current_connection_model.h"
#include "profiling_userdata_view.h"
#include "profiling_userdata_view_model.h"
#include "profiling_view_model.h"

namespace Ui
{
    class ProfilingView;
}

/// The interface used to collect RGP traces and view recent traces.
class ProfilingView final : public SplitClientFileView
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] userdata_view The userdata view.
    /// @param [in] view_model The view model used by this view.
    /// @param [in] userdata_view_model The user data view model used by this view.
    /// @param [in] parent The parent widget
    explicit ProfilingView(ProfilingUserdataView*                             userdata_view,
                           const std::shared_ptr<ProfilingViewModel>&         view_model,
                           const std::shared_ptr<ProfilingUserdataViewModel>& userdata_view_model,
                           QWidget*                                           parent = nullptr);

    /// @brief Destructor
    ~ProfilingView() override;

private slots:
    /// @brief Handles response to a profile validation failure being detected in a trace file.
    /// @param [in] path   The path of the file where the validation failure was detected.
    /// @param [in] status The specific validation failure (e.g. missing required chunk, SPM error, parser error).
    void OnInvalidTraceCounterData(const QString& path, RgpFileValidatorStatus status);

    /// @brief Handles response to current connections changing.
    /// @param [in] connections The current connections.
    void OnCurrentConnectionsChanged(std::unordered_map<DDConnectionId, devtrace::Api> connections);

    /// @brief Handles response to capture mode selection changing.
    /// @param [in] index The index of the newly selected capture mode.
    void OnCaptureModeSelectionChanged(int index);

    /// @brief Handles response to capture target selection changing.
    /// @param [in] index The index of the newly selected capture target.
    void OnCaptureTargetSelectionChanged(int index);

    /// @brief Called when trying to open a file and the app executable is missing.
    /// @param [in] path The path where the app executable was expected.
    void ApplicationExecutableMissing(const QString& path);

    void OnUserDataShortcutChanged(const GlobalShortcut& shortcut);

    /// @brief Called when the current capture mode changes.
    /// @param [in] capture_mode The new capture mode.
    void OnCurrentCaptureModeChanged(uint32_t capture_mode);

    /// @brief Called when the render op count spinbox value changes.
    /// @param [in] count The new render op count.
    void OnRenderOpCountBoxChanged(int count);

    /// @brief Called when the draw count changes from the model.
    /// @param [in] draw_count The new draw count.
    void OnDrawCountChanged(uint32_t draw_count);

    /// @brief Called when the dispatch count changes from the model.
    /// @param [in] dispatch_count The new dispatch count.
    void OnDispatchCountChanged(uint32_t dispatch_count);

    /// @brief Handle response to set default capture mode clicked
    void OnSetDefaultCaptureModeClicked();

    /// @brief Called when a trace fails to capture.
    void OnTraceFailed();

    /// @brief Called when a trace is aborted.
    void OnTraceAborted();

    /// @brief Called when a trace is ended (failure or success).
    void OnTraceEnded();

    /// @brief Called when the list of available capture modes for a client changes
    /// @param modes The available capture modes
    void OnAvailableCaptureModesChanged(const std::vector<CaptureMode>& modes);

    /// @brief Called in response to view model Ui status enablement change
    /// @param [in] should_enable Ui should be enabled or disabled.
    void OnUiStatusChanged(bool should_enable);

    /// @brief Enable the capture UI elements and disable the progress UI elements.
    void OnEnableCaptureUi();

    /// @brief Enable the progress UI elements and disable the capture UI elements.
    void OnEnableProgressUi();

    /// @brief Called when the prelaunch settings editable state needs to be updated based on connection status.
    /// @param [in] connections The current connections.
    void OnPrelaunchSettingsEditableStateChanged(std::unordered_map<DDConnectionId, devtrace::Api> connections);

    /// @brief Called when the connected process text changes, used to update prelaunch settings on disconnect.
    /// @param [in] connected_process_text The new connected process text.
    void OnConnectedProcessTextChanged(const QString& connected_process_text);

    /// @brief Called when the auto capture settings editable state changes.
    /// @param [in] enabled true if auto capture settings should be editable, false otherwise.
    void OnAutoCaptureSettingsEditableChanged(bool enabled);

private:
    /// @brief Binds the model to the view.
    void SetupConnections();

    /// @brief Adds specialized capture options
    void AddSpecializedCaptureOptions() const;

    ModelBinder                               model_binder_;                   ///< Utility object used to bind to a model.
    std::unique_ptr<Ui::ProfilingView>        ui_;                             ///< The UI for this view.
    std::weak_ptr<ProfilingViewModel>         view_model_;                     ///< View model.
    std::weak_ptr<ProfilingUserdataViewModel> userdata_view_model_;            ///< Userdata view model.
    ProfilingUserdataView*                    userdata_view_;                  ///< Userdata view.
    CurrentConnectionModel*                   current_connection_model_;       ///< The model that manages the current connections.
    QStringListModel*                         available_capture_modes_model_;  ///< The model that has the available capture modes.
};

#endif
