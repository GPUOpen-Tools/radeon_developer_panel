// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RGP profiling module utility view class implementation

#include "profiling_userdata_view.h"

#include <array>
#include <cstdint>

#include <QDesktopServices>
#include <QFileDialog>
#include <QJsonDocument>
#include <QStandardPaths>

#include <qt_common/custom_widgets/message_overlay.h>

#include "common/inc/collapsible_pane.h"
#include "profiling_module_definitions.h"

#include "profiling_userdata_view_model.h"

#include "common/inc/definitions.h"
#include "ui_profiling_auto_capture.h"
#include "ui_profiling_capture.h"
#include "ui_profiling_shader_instrumentation.h"
#include "ui_profiling_spm_counters.h"
#include "ui_profiling_sqtt.h"

static const QStringList kSqttProfileNames = {"Minimum", "Low", "Default", "High", "Maximum"};  ///< The names of the different SQTT profiles.

static const QStringList kAutoCaptureModes = {"None", "Frame index", "Dispatch range", "Dispatch timer"};

// Tooltip strings for shader instrumentation checkbox
static constexpr auto kShaderInstrumentationTooltipPublic =
    "When enabled, additional instrumentation will be applied to shaders to gather more detailed performance information.\n"
    "On RDNA4 or newer hardware, this also enables execution mask tracking.\n"
    "This may affect application performance.";

ProfilingUserdataView::ProfilingUserdataView(QWidget* parent)
    : QWidget(parent)
    , capture_ui_(new Ui::ProfilingCapture)
    , spm_ui_(new Ui::ProfilingSpmCounters)
    , sqtt_ui_(new Ui::ProfilingSqtt)
    , auto_capture_ui_(new Ui::ProfilingAutoCapture)
    , instrumentation_ui_(new Ui::ProfilingShaderInstrumentation)
{
    SetupUi();

    connect(auto_capture_ui_->auto_capture_combo_box,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ProfilingUserdataView::OnAutoCaptureModeDropdownChanged);

    connect(
        auto_capture_ui_->timing_ms_spin_box, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProfilingUserdataView::OnComputeAutoCaptureTimeBoxChanged);
    connect(auto_capture_ui_->dispatch_count_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProfilingUserdataView::OnDispatchCountBoxChanged);

    connect(auto_capture_ui_->start_dispatch_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProfilingUserdataView::OnDispatchStartBoxChanged);
    connect(
        sqtt_ui_->sqtt_buffer_size, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProfilingUserdataView::OnSqttBufferSizeProfileDropdownChanged);

    connect(auto_capture_ui_->frame_number_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &ProfilingUserdataView::OnFrameCaptureIndexBoxChanged);
}

ProfilingUserdataView::~ProfilingUserdataView() noexcept = default;

Ui::ProfilingCapture* ProfilingUserdataView::GetCaptureUi() const
{
    return capture_ui_.get();
}

void ProfilingUserdataView::SetupUi()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setAlignment(Qt::AlignTop);
    main_layout->setSpacing(25);
    setLayout(main_layout);

    const auto auto_capture = new CollapsiblePane<QWidget>(this);
    auto_capture->SetTitleText("Auto capture");
    auto_capture->Collapse();

    auto_capture_ui_->setupUi(auto_capture->GetBody());
    main_layout->addWidget(auto_capture);

    const auto capture_settings = new CollapsiblePane<QWidget>(this);
    capture_settings->SetTitleText("Advanced settings");
    capture_settings->Collapse();
    main_layout->addWidget(capture_settings);

    const auto vertical_layout = new QVBoxLayout;
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->setSpacing(30);
    capture_settings->GetBody()->setLayout(vertical_layout);

    const auto sqtt = new QWidget;
    sqtt_ui_->setupUi(sqtt);
    vertical_layout->addWidget(sqtt);

    const auto shader_instrumentation = new QWidget;
    instrumentation_ui_->setupUi(shader_instrumentation);
    sqtt_ui_->verticalLayout_2->addWidget(shader_instrumentation);

    instrumentation_ui_->enable_instrumentation_info->setToolTip(kShaderInstrumentationTooltipPublic);

    auto_capture_ui_->auto_capture_combo_box->addItems(kAutoCaptureModes);
    auto_capture_ui_->frame_number_spinbox->setMinimum(kFrameIndexMinimum);
    auto_capture_ui_->frame_number_spinbox->setMaximum(INT_MAX);

    auto_capture_ui_->start_dispatch_spinbox->setMinimum(kDispatchIndexMinimum);

    Q_ASSERT(kSqttProfileNames.size() == static_cast<int>(devtrace::SqttBufferSizeProfiles::kMaximum) + 1);

    sqtt_ui_->sqtt_buffer_size->addItems(kSqttProfileNames);
    sqtt_ui_->sqtt_custom_buffer_size->hide();
    sqtt_ui_->sqtt_custom_buffer_size->setMinimum(1);
    sqtt_ui_->sqtt_custom_buffer_size->setMaximum(1024);

    auto_capture_ui_->start_dispatch_spinbox->setMaximum(INT_MAX);
}

void ProfilingUserdataView::SetModel(const std::shared_ptr<ProfilingUserdataViewModel>& view_model)
{
    model_binder_.StartBinding();

    view_model_ = view_model;

    connect(capture_ui_->enable_instruction_tracing, &QCheckBox::checkStateChanged, this, &ProfilingUserdataView::OnEnableInstructionTracingCheckStateChanged);
    connect(capture_ui_->enable_spm_checkbox, &QCheckBox::checkStateChanged, this, &ProfilingUserdataView::OnEnableSpmCaptureCheckStateChanged);
    connect(capture_ui_->enable_legacy_capture, &QCheckBox::checkStateChanged, this, &ProfilingUserdataView::OnEnableLegacyCaptureCheckStateChanged);
    connect(capture_ui_->disable_capture_timeout, &QCheckBox::checkStateChanged, this, &ProfilingUserdataView::OnDisableCaptureTimeoutCheckStateChanged);

    connect(instrumentation_ui_->enable_exec_pop_tokens_check_box,
            &QCheckBox::checkStateChanged,
            this,
            &ProfilingUserdataView::OnEnableExecPopTokensCheckStateChanged);

    connect(instrumentation_ui_->enable_instrumentation_check_box,
            &QCheckBox::checkStateChanged,
            this,
            &ProfilingUserdataView::OnEnableShaderInstrumentationCheckStateChanged);

    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::LegacyCaptureEnabledChanged, this, &ProfilingUserdataView::OnEnableLegacyCaptureChanged);
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::DisableCaptureTimeoutChanged, this, &ProfilingUserdataView::OnDisableCaptureTimeoutChanged);
    model_binder_.Connect(view_model_.get(),
                          &ProfilingUserdataViewModel::ShaderInstrumentationEnabledChanged,
                          this,
                          &ProfilingUserdataView::OnEnableShaderInstrumentationChanged);
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::InstructionTracingEnabledChanged, this, &ProfilingUserdataView::OnEnableInstructionTracingChanged);
    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::SpmCaptureEnabledChanged, this, &ProfilingUserdataView::OnEnableSpmCaptureChanged);
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::SpmCaptureSupportedChanged, this, &ProfilingUserdataView::OnSpmCaptureSupportedChanged);
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::SqttProfileIndexChanged, this, &ProfilingUserdataView::OnSqttBufferSizeProfileChanged);
    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::AutoCaptureModeChanged, this, &ProfilingUserdataView::OnAutoCaptureModeChanged);
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::ComputeAutoCaptureTimeChanged, this, &ProfilingUserdataView::OnComputeAutoCaptureTimeChanged);
    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::DispatchStartChanged, this, &ProfilingUserdataView::OnDispatchStartChanged);
    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::DispatchCountChanged, this, &ProfilingUserdataView::OnDispatchCountChanged);

    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::FrameCaptureIndexChanged, this, &ProfilingUserdataView::OnFrameCaptureIndexChanged);

    // Connect to prelaunch settings editable signal
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::PrelaunchSettingsEditableChanged, this, &ProfilingUserdataView::OnPrelaunchSettingsEditableChanged);

    model_binder_.Connect(view_model_.get(),
                          &ProfilingUserdataViewModel::EditShaderInstrumentationEnabledChanged,
                          this,
                          &ProfilingUserdataView::OnEditShaderInstrumentationEnabledChanged);

    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::ShaderInstrumentationFailedToSet, this, &ProfilingUserdataView::OnShaderInstrumentationFailedToSet);

    model_binder_.Connect(view_model_.get(), &ProfilingUserdataViewModel::ExecPopTokensEnabledChanged, this, &ProfilingUserdataView::OnExecPopTokensChanged);

    // In public build, hide the exec/pop tokens checkbox and info button as it will be automatically
    // controlled by the shader instrumentation checkbox
    instrumentation_ui_->enable_exec_pop_tokens_check_box->hide();
    instrumentation_ui_->enable_exec_pop_tokens_info->hide();

    // Connect to auto capture settings editable signal
    model_binder_.Connect(
        view_model_.get(), &ProfilingUserdataViewModel::AutoCaptureSettingsEditableChanged, this, &ProfilingUserdataView::OnAutoCaptureSettingsEditableChanged);

    view_model_->OnBind();
}

void ProfilingUserdataView::OnEnableInstructionTracingChanged(bool enabled) const
{
    capture_ui_->enable_instruction_tracing->setChecked(enabled);
}

void ProfilingUserdataView::OnEnableInstructionTracingCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleInstructionTracingEnabledChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnEnableSpmCaptureChanged(const bool enabled) const
{
    capture_ui_->enable_spm_checkbox->setChecked(enabled);
}

void ProfilingUserdataView::OnEnableSpmCaptureCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleSpmCaptureEnabledChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnSpmCaptureSupportedChanged(const bool supported) const
{
    capture_ui_->enable_spm_checkbox->setVisible(supported);
}

void ProfilingUserdataView::OnEnableShaderInstrumentationChanged(const bool enabled) const
{
    instrumentation_ui_->enable_instrumentation_check_box->setChecked(enabled);
}

void ProfilingUserdataView::OnEnableShaderInstrumentationCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleShaderInstrumentationEnabledChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnEnableLegacyCaptureChanged(const bool enabled) const
{
    capture_ui_->enable_legacy_capture->setChecked(enabled);
}

void ProfilingUserdataView::OnEnableLegacyCaptureCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleLegacyCaptureEnabledChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnDisableCaptureTimeoutChanged(const bool disabled) const
{
    capture_ui_->disable_capture_timeout->setChecked(disabled);
}

void ProfilingUserdataView::OnDisableCaptureTimeoutCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleDisableCaptureTimeoutChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnEnableExecPopTokensCheckStateChanged(const Qt::CheckState state) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleExecPopTokensEnabledChanged(state == Qt::Checked);
    }
}

void ProfilingUserdataView::OnSqttBufferSizeProfileDropdownChanged(int index) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleSqttProfileIndexChanged(static_cast<devtrace::SqttBufferSizeProfiles>(index));
    }
}

void ProfilingUserdataView::OnSqttBufferSizeProfileChanged(devtrace::SqttBufferSizeProfiles profile) const
{
    sqtt_ui_->sqtt_buffer_size->setCurrentIndex(static_cast<int>(profile));
}

void ProfilingUserdataView::OnAutoCaptureModeDropdownChanged(int index) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleAutoCaptureModeChanged(static_cast<devtrace::AutoCaptureMode>(index));
    }
}

void ProfilingUserdataView::OnAutoCaptureModeChanged(const devtrace::AutoCaptureMode mode) const
{
    auto_capture_ui_->auto_capture_combo_box->setCurrentIndex(mode);

    const bool is_using_frame_index_capture = mode == devtrace::AutoCaptureMode::kAutoCaptureModeFrameIndex;
    auto_capture_ui_->frame_number_label->setVisible(is_using_frame_index_capture);
    auto_capture_ui_->frame_number_spinbox->setVisible(is_using_frame_index_capture);

    const bool is_using_timed_capture = mode == devtrace::AutoCaptureMode::kAutoCaptureModeTimer;
    auto_capture_ui_->capture_time_label->setVisible(is_using_timed_capture);
    auto_capture_ui_->timing_ms_spin_box->setVisible(is_using_timed_capture);

    const bool is_using_range_capture = mode == devtrace::AutoCaptureMode::kAutoCaptureModeDispatchIndices;
    auto_capture_ui_->start_dispatch_label->setVisible(is_using_range_capture);
    auto_capture_ui_->start_dispatch_spinbox->setVisible(is_using_range_capture);

    auto_capture_ui_->dispatch_count_label->setVisible(is_using_range_capture || is_using_timed_capture);
    auto_capture_ui_->dispatch_count_spinbox->setVisible(is_using_range_capture || is_using_timed_capture);

    if (!is_using_frame_index_capture)
    {
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->frame_number_label);
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->frame_number_spinbox);
    }

    if (!is_using_range_capture || !is_using_timed_capture)
    {
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->dispatch_count_label);
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->dispatch_count_spinbox);
    }

    if (!is_using_range_capture)
    {
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->start_dispatch_label);
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->start_dispatch_spinbox);
    }

    if (!is_using_timed_capture)
    {
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->capture_time_label);
        auto_capture_ui_->normal_capture_layout->removeWidget(auto_capture_ui_->timing_ms_spin_box);
    }

    switch (mode)
    {
    case devtrace::AutoCaptureMode::kAutoCaptureModeFrameIndex:
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->frame_number_label, 2, 0);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->frame_number_spinbox, 2, 1);
        break;

    case devtrace::AutoCaptureMode::kAutoCaptureModeTimer:
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->dispatch_count_label, 1, 0);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->dispatch_count_spinbox, 1, 1);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->capture_time_label, 2, 0);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->timing_ms_spin_box, 2, 1);
        break;

    case devtrace::AutoCaptureMode::kAutoCaptureModeDispatchIndices:
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->dispatch_count_label, 1, 0);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->dispatch_count_spinbox, 1, 1);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->start_dispatch_label, 2, 0);
        auto_capture_ui_->normal_capture_layout->addWidget(auto_capture_ui_->start_dispatch_spinbox, 2, 1);
        break;
    case devtrace::AutoCaptureMode::kAutoCaptureModeNone:
    default:
        break;
    }
}

void ProfilingUserdataView::OnComputeAutoCaptureTimeBoxChanged(int time_ms) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleComputeAutoCaptureTimeChanged(static_cast<uint32_t>(time_ms));
    }
}

void ProfilingUserdataView::OnComputeAutoCaptureTimeChanged(uint32_t time_ms) const
{
    auto_capture_ui_->timing_ms_spin_box->setValue(static_cast<int>(time_ms));
}

void ProfilingUserdataView::OnDispatchStartBoxChanged(int dispatch_start) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleDispatchStartChanged(static_cast<uint32_t>(dispatch_start));
    }
}

void ProfilingUserdataView::OnDispatchStartChanged(int dispatch_start) const
{
    auto_capture_ui_->start_dispatch_spinbox->setValue(static_cast<int>(dispatch_start));
}

void ProfilingUserdataView::OnDispatchCountBoxChanged(int dispatch_count) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleDispatchCountChanged(static_cast<uint32_t>(dispatch_count));
    }
}

void ProfilingUserdataView::OnDispatchCountChanged(int dispatch_count) const
{
    auto_capture_ui_->dispatch_count_spinbox->setValue(static_cast<int>(dispatch_count));
}

void ProfilingUserdataView::OnFrameCaptureIndexBoxChanged(int frame_index) const
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleFrameCaptureIndexChanged(static_cast<uint32_t>(frame_index));
    }
}

void ProfilingUserdataView::OnFrameCaptureIndexChanged(uint32_t frame_index) const
{
    auto_capture_ui_->frame_number_spinbox->setValue(static_cast<int>(frame_index));
}

void ProfilingUserdataView::OnShaderInstrumentationFailedToSet()
{
    MessageOverlay::CriticalAsync(
        "Unable to toggle shader instrumentation",
        "An unexpected error was encountered toggling shader instrumentation. Please relaunch Radeon Developer Panel to continue using this feature.");
}

void ProfilingUserdataView::OnExecPopTokensChanged(bool enabled)
{
    if (instrumentation_ui_->enable_exec_pop_tokens_check_box)
    {
        instrumentation_ui_->enable_exec_pop_tokens_check_box->setChecked(enabled);
    }
}

void ProfilingUserdataView::OnExecPopTokensSupportedChanged([[maybe_unused]] bool supported)
{
}

void ProfilingUserdataView::OnPrelaunchSettingsEditableChanged(bool enabled) const
{
    // Only legacy capture and shader instrumentation are disabled when an app is connected
    capture_ui_->enable_legacy_capture->setEnabled(enabled);

    // Auto capture combo box is also a prelaunch setting that must be configured before the app connects
    auto_capture_ui_->auto_capture_combo_box->setEnabled(enabled);
}

void ProfilingUserdataView::OnEditShaderInstrumentationEnabledChanged(bool enabled) const
{
    instrumentation_ui_->enable_instrumentation_check_box->setEnabled(enabled);
}

void ProfilingUserdataView::OnAutoCaptureSettingsEditableChanged(bool enabled) const
{
    // Auto capture settings (frame index, dispatch range, dispatch timer) are editable when:
    // - prelaunch settings are editable (no app connected), OR
    // - auto-capture mode is None
    auto_capture_ui_->frame_number_spinbox->setEnabled(enabled);
    auto_capture_ui_->timing_ms_spin_box->setEnabled(enabled);
    auto_capture_ui_->start_dispatch_spinbox->setEnabled(enabled);
    auto_capture_ui_->dispatch_count_spinbox->setEnabled(enabled);

    // Capture settings checkboxes
    capture_ui_->enable_instruction_tracing->setEnabled(enabled);
    capture_ui_->enable_spm_checkbox->setEnabled(enabled);
    capture_ui_->disable_capture_timeout->setEnabled(enabled);

    // SQTT buffer size settings
    sqtt_ui_->sqtt_buffer_size->setEnabled(enabled);
}
