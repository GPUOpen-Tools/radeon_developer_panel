// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Class analysis utility view definition.

#include "crash_analysis_userdata_view.h"

#include <QWidget>

#include "common/inc/collapsible_pane.h"

#include "ui_crash_analysis_advanced.h"
#include "ui_crash_analysis_auto_summary_generation.h"
#include "ui_crash_analysis_enhanced_crash.h"
#include "ui_crash_analysis_symbols.h"

CrashAnalysisUserdataView::CrashAnalysisUserdataView()
    : summary_ui_(new Ui::CrashAnalysisAutoSummaryGeneration)
    , advanced_ui_(new Ui::CrashAnalysisAdvanced)
    , enhanced_ui_(new Ui::CrashAnalysisEnhancedCrash)
    , symbol_ui_(new Ui::CrashAnalysisSymbols)
    , pdb_path_dialog_(new IncludeDirectoriesView(";", "PDB Search Paths", {}, this))
{
    pdb_path_dialog_->hide();

    const auto vertical_layout = new QVBoxLayout(this);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->setAlignment(Qt::AlignTop);
    vertical_layout->setSpacing(25);
    setLayout(vertical_layout);

    const auto summary_generation = new CollapsiblePane<QWidget>(this);
    summary_generation->SetTitleText("Automatic summary generation");

    summary_ui_->setupUi(summary_generation->GetBody());
    vertical_layout->addWidget(summary_generation);

    const auto advanced_options = new CollapsiblePane<QWidget>(this);
    advanced_options->SetTitleText("Summary generation options");

    advanced_ui_->setupUi(advanced_options->GetBody());
    vertical_layout->addWidget(advanced_options);

    const auto enhanced_crash = new CollapsiblePane<QWidget>(this);
    enhanced_crash->SetTitleText("Analysis options");

    enhanced_ui_->setupUi(enhanced_crash->GetBody());
    vertical_layout->addWidget(enhanced_crash);

    // Cache the default info-button tooltips so they can be restored when GPR capture is supported.
    sgpr_info_default_tooltip_ = enhanced_ui_->collection_sgpr_info->toolTip();
    vgpr_info_default_tooltip_ = enhanced_ui_->collection_vgpr_info->toolTip();

    // Non-internal build: hide serialize ops
    enhanced_ui_->disable_serialize_alu_ops->hide();
    enhanced_ui_->disable_serialize_mem_ops->hide();

    const auto symbols = new CollapsiblePane<QWidget>(this);
    symbols->SetTitleText("Shader debug information search paths");

    symbol_ui_->setupUi(symbols->GetBody());
    vertical_layout->addWidget(symbols);
}

CrashAnalysisUserdataView::~CrashAnalysisUserdataView() = default;

void CrashAnalysisUserdataView::SetModel(const std::shared_ptr<CrashAnalysisUserdataViewModel>& view_model)
{
    model_binder_.StartBinding();
    view_model_ = view_model;

    // Summary generation signals (view model -> view)
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::GenerateTextSummaryChanged, this, &CrashAnalysisUserdataView::OnGenerateTextSummaryChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::GenerateJsonSummaryChanged, this, &CrashAnalysisUserdataView::OnGenerateJsonSummaryChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::ShowMarkerSourceChanged, this, &CrashAnalysisUserdataView::OnShowMarkerSourceChanged);
    model_binder_.Connect(view_model.get(), &CrashAnalysisUserdataViewModel::ExpandMarkersChanged, this, &CrashAnalysisUserdataView::OnExpandMarkersChanged);
    model_binder_.Connect(view_model.get(),
                          &CrashAnalysisUserdataViewModel::HardwareCrashAnalysisSupportedChanged,
                          this,
                          &CrashAnalysisUserdataView::OnHardwareCrashAnalysisSupportedChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::GprCaptureSupportedChanged, this, &CrashAnalysisUserdataView::OnGprCaptureSupportedChanged);

    // Summary generation checkbox connections (view -> view model)
    model_binder_.Connect(
        summary_ui_->text_crash_data_summary, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleGenerateTextSummaryChanged);
    model_binder_.Connect(
        summary_ui_->json_crash_data_summary, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleGenerateJsonSummaryChanged);
    model_binder_.Connect(
        advanced_ui_->marker_source, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleShowMarkerSourceChanged);
    model_binder_.Connect(
        advanced_ui_->expand_markers, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleExpandMarkersChanged);

    // Enhanced crash analysis signals (view model -> view)
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::EnableEnhancedCrashChanged, this, &CrashAnalysisUserdataView::OnEnableEnhancedCrashChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::DisableSerializeMemOpsChanged, this, &CrashAnalysisUserdataView::OnDisableSerializeMemOpsChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::DisableSerializeAluOpsChanged, this, &CrashAnalysisUserdataView::OnDisableSerializeAluOpsChanged);

    // Enhanced crash analysis checkbox connections (view -> view model)
    model_binder_.Connect(
        enhanced_ui_->enable_enhanced_crash, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleEnableEnhancedCrashChanged);
    model_binder_.Connect(enhanced_ui_->disable_serialize_mem_ops,
                          &QCheckBox::stateChanged,
                          view_model.get(),
                          &CrashAnalysisUserdataViewModel::HandleDisableSerializeMemOpsChanged);
    model_binder_.Connect(enhanced_ui_->disable_serialize_alu_ops,
                          &QCheckBox::stateChanged,
                          view_model.get(),
                          &CrashAnalysisUserdataViewModel::HandleDisableSerializeAluOpsChanged);

    // SGPR/VGPR signals (view model -> view)
    model_binder_.Connect(view_model.get(), &CrashAnalysisUserdataViewModel::CollectSgprsChanged, this, &CrashAnalysisUserdataView::OnCollectSgprsChanged);
    model_binder_.Connect(view_model.get(), &CrashAnalysisUserdataViewModel::CollectVgprsChanged, this, &CrashAnalysisUserdataView::OnCollectVgprsChanged);

    // SGPR/VGPR checkbox connections (view -> view model)
    model_binder_.Connect(
        enhanced_ui_->collect_sgpr_checkbox, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleCollectSgprsChanged);
    model_binder_.Connect(
        enhanced_ui_->collect_vgpr_checkbox, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandleCollectVgprsChanged);

    // PDB paths signals (view model -> view)
    model_binder_.Connect(view_model.get(), &CrashAnalysisUserdataViewModel::PdbSearchPathsChanged, this, &CrashAnalysisUserdataView::OnPdbSearchPathsChanged);
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::PdbIncludeSubfoldersChanged, this, &CrashAnalysisUserdataView::OnPdbIncludeSubfoldersChanged);

    // PDB paths button
    model_binder_.Connect(symbol_ui_->edit_pdb_paths_button, &QPushButton::pressed, this, &CrashAnalysisUserdataView::EditPdbPaths);

    // PDB include subfolders checkbox (view -> view model)
    model_binder_.Connect(
        symbol_ui_->pdb_include_subfolders, &QCheckBox::stateChanged, view_model.get(), &CrashAnalysisUserdataViewModel::HandlePdbIncludeSubfoldersChanged);

    // PDB dialog confirmation
    connect(pdb_path_dialog_, &IncludeDirectoriesView::OKButtonClicked, this, &CrashAnalysisUserdataView::OnPdbSearchDialogConfirmed);

    // Application connected signal (view model -> view)
    model_binder_.Connect(
        view_model.get(), &CrashAnalysisUserdataViewModel::ApplicationConnectedChanged, this, &CrashAnalysisUserdataView::OnApplicationConnectedChanged);

    view_model_->OnBind();
}

void CrashAnalysisUserdataView::OnPdbSearchDialogConfirmed(const QStringList& paths) const
{
    const QString path_string = paths.join(";");
    view_model_->HandlePdbSearchPathsChanged(path_string);
}

void CrashAnalysisUserdataView::OnGenerateTextSummaryChanged(const bool generate_text_summary) const
{
    summary_ui_->text_crash_data_summary->setChecked(generate_text_summary);
}

void CrashAnalysisUserdataView::OnGenerateJsonSummaryChanged(const bool generate_json_summary) const
{
    summary_ui_->json_crash_data_summary->setChecked(generate_json_summary);
}

void CrashAnalysisUserdataView::OnShowMarkerSourceChanged(const bool show_marker_source) const
{
    advanced_ui_->marker_source->setChecked(show_marker_source);
}

void CrashAnalysisUserdataView::OnExpandMarkersChanged(const bool expand_markers) const
{
    advanced_ui_->expand_markers->setChecked(expand_markers);
}

void CrashAnalysisUserdataView::EditPdbPaths() const
{
    pdb_path_dialog_->setWindowState(pdb_path_dialog_->windowState() & ~Qt::WindowMinimized);
    pdb_path_dialog_->exec();
}

void CrashAnalysisUserdataView::UpdateEnhancedCrashSubOptionsEnabled() const
{
    // Sub-options are enabled only if hardware crash analysis is supported AND the enable_enhanced_crash checkbox is checked AND no application is connected
    const bool sub_options_enabled = hardware_crash_analysis_supported_ && enhanced_ui_->enable_enhanced_crash->isChecked() && !application_connected_;

    enhanced_ui_->disable_serialize_mem_ops->setEnabled(sub_options_enabled);
    enhanced_ui_->disable_serialize_alu_ops->setEnabled(sub_options_enabled);

    // SGPR/VGPR collection additionally requires a driver new enough to emit the data. On older drivers
    // the flags are silently accepted but no GPR data is written, so disable and explain instead.
    const bool gpr_options_enabled = sub_options_enabled && gpr_capture_supported_;

    enhanced_ui_->collect_sgpr_checkbox->setEnabled(gpr_options_enabled);
    enhanced_ui_->collect_vgpr_checkbox->setEnabled(gpr_options_enabled);

    // A disabled QWidget does not receive hover events, so a tooltip set on the grayed-out checkbox would
    // never show. Instead, surface the "unsupported" message on the companion info buttons, which we keep
    // enabled (when the parent options are otherwise available) so the indicator remains hoverable.
    // Info buttons always track sub_options_enabled regardless of GPR support — only the tooltip differs.
    enhanced_ui_->collection_sgpr_info->setEnabled(sub_options_enabled);
    enhanced_ui_->collection_vgpr_info->setEnabled(sub_options_enabled);

    if (gpr_capture_supported_)
    {
        enhanced_ui_->collection_sgpr_info->setToolTip(sgpr_info_default_tooltip_);
        enhanced_ui_->collection_vgpr_info->setToolTip(vgpr_info_default_tooltip_);
    }
    else
    {
        const QString unsupported_tooltip = QStringLiteral("Wave SGPR/VGPR collection requires AMD driver 25.20 or newer.");
        enhanced_ui_->collection_sgpr_info->setToolTip(unsupported_tooltip);
        enhanced_ui_->collection_vgpr_info->setToolTip(unsupported_tooltip);
    }
}

void CrashAnalysisUserdataView::OnHardwareCrashAnalysisSupportedChanged(const bool supported)
{
    hardware_crash_analysis_supported_ = supported;

    // Enable enhanced crash checkbox only if supported AND not connected to an application
    enhanced_ui_->enable_enhanced_crash->setEnabled(supported && !application_connected_);

    enhanced_ui_->enable_enhanced_crash->setToolTip("");
    if (!supported)
    {
        enhanced_ui_->enable_enhanced_crash->setToolTip("Unsupported with current driver or hardware configuration.");
        enhanced_ui_->enable_enhanced_crash->setChecked(false);
    }

    // Update sub-options based on new support status
    UpdateEnhancedCrashSubOptionsEnabled();
}

void CrashAnalysisUserdataView::OnGprCaptureSupportedChanged(const bool supported)
{
    gpr_capture_supported_ = supported;

    // Clear the selections when support is unavailable so the effective capture config in
    // RgdTraceSourceConfig matches the UI state. Without this a checked-but-disabled box
    // would still request SGPR/VGPR collection from the driver. Because is_gpr_capture_supported
    // is a driver-version property (stable for the lifetime of a connection), this fires only
    // once on connect with an old driver and does not wipe preferences on HCA state changes.
    if (!supported)
    {
        enhanced_ui_->collect_sgpr_checkbox->setChecked(false);
        enhanced_ui_->collect_vgpr_checkbox->setChecked(false);
    }

    UpdateEnhancedCrashSubOptionsEnabled();
}

void CrashAnalysisUserdataView::OnEnableEnhancedCrashChanged(const bool enabled) const
{
    enhanced_ui_->enable_enhanced_crash->setChecked(enabled);

    // Update sub-options based on new enabled state
    UpdateEnhancedCrashSubOptionsEnabled();
}

void CrashAnalysisUserdataView::OnDisableSerializeMemOpsChanged(const bool disabled) const
{
    enhanced_ui_->disable_serialize_mem_ops->setChecked(disabled);
}

void CrashAnalysisUserdataView::OnDisableSerializeAluOpsChanged(const bool disabled) const
{
    enhanced_ui_->disable_serialize_alu_ops->setChecked(disabled);
}

void CrashAnalysisUserdataView::OnCollectSgprsChanged(const bool collect) const
{
    enhanced_ui_->collect_sgpr_checkbox->setChecked(collect);
}

void CrashAnalysisUserdataView::OnCollectVgprsChanged(const bool collect) const
{
    enhanced_ui_->collect_vgpr_checkbox->setChecked(collect);
}

void CrashAnalysisUserdataView::OnPdbSearchPathsChanged(const QString& paths) const
{
    symbol_ui_->pdb_line_edit->setText(paths);
    QString tooltip_paths = paths;
    tooltip_paths.replace(";", "\n");
    symbol_ui_->pdb_line_edit->setToolTip(tooltip_paths);
    pdb_path_dialog_->SetListItems(paths);
}

void CrashAnalysisUserdataView::OnPdbIncludeSubfoldersChanged(const bool include_subfolders) const
{
    symbol_ui_->pdb_include_subfolders->setChecked(include_subfolders);
}

void CrashAnalysisUserdataView::OnApplicationConnectedChanged(const bool connected)
{
    application_connected_ = connected;

    // Enable/disable the main HCA checkbox based on connection status and hardware support
    enhanced_ui_->enable_enhanced_crash->setEnabled(hardware_crash_analysis_supported_ && !connected);

    // Update sub-options based on new connection state
    UpdateEnhancedCrashSubOptionsEnabled();
}
