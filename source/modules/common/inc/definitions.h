// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Shared definitions for the Developer Panel.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_DEFINITIONS_H_
#define RDP_SOURCE_MODULES_COMMON_INC_DEFINITIONS_H_

#define MODULE_DATA_CHANGE_EVENT (QEvent::User + 1)

#define Q_RETAIN_SIZE_POLICY(w)                 \
                                                \
    {                                           \
        QSizePolicy policy = (w)->sizePolicy(); \
        policy.setRetainSizeWhenHidden(true);   \
        (w)->setSizePolicy(policy);             \
    }

static const constexpr char* kProfileExtension                = "rgp";  ///< The extension used when saving RGP trace files.
static constexpr const char* kDashText                        = "-";
static constexpr const char* kBrowseTraceDirectoryCaptionText = "Choose profile output directory...";
static constexpr const char* kBrowseTextEditorCaptionText     = "Choose a text editor application...";

static constexpr const char* kEnableRdfInspectorEnv = "RDP_ENABLE_RDF_INSPECTOR";

static constexpr const char* kRgpExeMissingDialogTitle                                = "Radeon GPU Profiler missing";
static constexpr const char* kRmvExeMissingDialogTitle                                = "Radeon Memory Visualizer missing";
static constexpr const char* kRraExeMissingDialogTitle                                = "Radeon Raytracing Analyzer missing";
static constexpr const char* kRgdExeMissingDialogTitle                                = "Radeon GPU Detective missing";
static constexpr const char* kTxtEditorExeMissingDialogTitle                          = "Text editor missing";
static constexpr const char* kRgpExeMissingDialogMessage                              = "The Radeon GPU Profiler executable cannot be found at \"%1\".";
static constexpr const char* kRmvExeMissingDialogMessage                              = "The Radeon Memory Visualizer executable cannot be found at \"%1\".";
static constexpr const char* kRraExeMissingDialogMessage                              = "The Radeon Raytracing Analyzer executable cannot be found at \"%1\".";
static constexpr const char* kRgdExeMissingDialogMessage                              = "The Radeon GPU Detective executable cannot be found at \"%1\".";
static constexpr const char* kTxtEditExeMissingDialogMessage                          = "A text editor executable cannot be found at \"%1\".";
static constexpr const char* kTraceProgressReceived                                   = "%1/%2";
static constexpr const char* kTraceDirectoryNotWritableDialogTitle                    = "Target directory not writable";
static constexpr const char* kTraceDirectoryNotValidDialogTitle                       = "Target directory not valid";
static constexpr const char* kTraceDirectoryNotWritableDialogMessage                  = "Directory \"%1\" is not writable.";
static constexpr const char* kTraceDirectoryNotValidDialogMessage                     = "Directory \"%1\" is not valid.";
static constexpr const char* kProfilingConfigSummaryGenericMessage                    = "All target applications will be profiled ";
static constexpr const char* kProfilingConfigSummaryDispatchRangeMessage              = "from Dispatch %1 to Dispatch %2 when launched.";
static constexpr const char* kProfilingConfigSummaryFrameNumberMessage                = "when frame %1 is rendered.";
static constexpr const char* kProfilingConfigSummaryTriggerCaptureMessage             = "when the capture button is clicked.";
static constexpr const char* kProfilingConfigSummaryTriggerComputeCaptureMessage      = "for %1 dispatches when the capture button is clicked.";
static constexpr const char* kProfilingConfigSummaryTriggerComputeTimerCaptureMessage = "for %1 dispatches %3ms after launch.";
static constexpr const char* kAppNameMacro                                            = "$(APP_NAME)";
static constexpr const char* kApplicationNameMacro                                    = "$(APPLICATION_NAME)";
static constexpr const char* kApiMacro                                                = "$(API)";
static constexpr const char* kRgpProfilesDefaultParentFolder                          = "rgp_profiles";
static constexpr const char* kRmvTracesDefaultParentFolder                            = "rmv_traces";
static constexpr const char* kRgdTracesDefaultParentFolder                            = "rgd_dumps";
static constexpr const char* kPipelinesDefaultParentFolder                            = "pipelines";
static constexpr const char* kRraScenesDefaultParentFolder                            = "rra_scenes";
static constexpr const char* kRedIndicator                                            = ":/red_light.svg";
static constexpr const char* kPanelMainWindow                                         = "RDPMainWindow";

enum class Api
{
    kUnknown,
    kVulkan,
    kDirectX12,
    kDirectX11,
    kDirectX9,
    kOpenCL,
    kOpenGL,
    kHip
};

#endif
