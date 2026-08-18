// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  RRA module utility view class implementation

#include "raytracing_userdata_view.h"
#include "ui_raytracing_capture.h"
#include "ui_raytracing_ray_history.h"

#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QValidator>

#include <source_userdata.h>

#include <common/inc/collapsible_pane.h>

#include "raytracing_module_definitions.h"

static const QStringList  kRayHistoryBufferProfileNames = {"Minimum", "Low", "Default", "High", "Maximum"};
static constexpr int32_t  kDefaultHistoryProfileIndex   = 2;
static constexpr uint64_t kConversionFactor             = 1000000000;
static constexpr double   kConversionFactorDbl          = 0.000000001;

RaytracingUserdataView::RaytracingUserdataView()
    : capture_ui_(new Ui::RaytracingCapture)
    , ray_history_ui_(new Ui::RaytracingRayHistory)
    , enable_marker_capture_checkbox_(nullptr)
    , marker_begin_edit_(nullptr)
    , marker_end_edit_(nullptr)
    , marker_capture_pane_(nullptr)
{
    SetupUi();
}

RaytracingUserdataView::~RaytracingUserdataView() noexcept = default;

void RaytracingUserdataView::SetupUi()
{
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setAlignment(Qt::AlignTop);
    main_layout->setSpacing(25);
    setLayout(main_layout);

    auto* ray_history = new CollapsiblePane<QWidget>(this);
    ray_history->SetTitleText("Ray history");
    ray_history_ui_->setupUi(ray_history->GetBody());

    ray_history_ui_->history_buffer_size->addItems(kRayHistoryBufferProfileNames);
    ray_history_ui_->history_buffer_size->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    ray_history_ui_->history_buffer_size->setCurrentIndex(kDefaultHistoryProfileIndex);

    auto* validator = new QDoubleValidator();
    validator->setBottom(kConversionFactorDbl);
    ray_history_ui_->history_buffer_size_custom_edit->setValidator(validator);

    ray_history_ui_->history_buffer_size_custom_edit->hide();
    ray_history_ui_->history_buffer_size_edit_units_label->hide();

    main_layout->addWidget(ray_history);

    // Add marker capture settings
    auto* marker_capture = new CollapsiblePane<QWidget>(this);
    marker_capture->SetTitleText("Marker capture");

    auto* marker_body   = marker_capture->GetBody();
    auto* marker_layout = new QVBoxLayout(marker_body);
    marker_layout->setContentsMargins(9, 0, 9, 9);

    enable_marker_capture_checkbox_ = new QCheckBox("Enable marker-based capture", marker_body);
    enable_marker_capture_checkbox_->setToolTip(
        "When enabled, capture will be triggered by user markers instead of capturing on button press.\n"
        "Insert the begin marker in your application to start capture, and the end marker to finish.");
    marker_layout->addWidget(enable_marker_capture_checkbox_);

    auto* marker_form = new QFormLayout();
    marker_form->setContentsMargins(0, 10, 0, 0);

    marker_begin_edit_ = new QLineEdit(marker_body);
    marker_begin_edit_->setPlaceholderText("RRABeginMarker");
    marker_begin_edit_->setToolTip("The marker string that starts the capture");
    marker_form->addRow("Begin marker:", marker_begin_edit_);

    marker_end_edit_ = new QLineEdit(marker_body);
    marker_end_edit_->setPlaceholderText("RRAEndMarker");
    marker_end_edit_->setToolTip("The marker string that ends the capture");
    marker_form->addRow("End marker:", marker_end_edit_);

    marker_layout->addLayout(marker_form);
    marker_body->setLayout(marker_layout);

    main_layout->addWidget(marker_capture);

    // Hide marker capture until driver support is confirmed
    marker_capture_pane_ = marker_capture;
    marker_capture_pane_->hide();
}

void RaytracingUserdataView::SetModel(const std::shared_ptr<RaytracingUserdataViewModel>& view_model)
{
    view_model_ = view_model;

    model_binder_.StartBinding();

    model_binder_.Connect(
        view_model_.get(), &RaytracingUserdataViewModel::RayHistoryBufferSizeChanged, this, &RaytracingUserdataView::OnRayHistoryBufferSizedChanged);
    model_binder_.Connect(
        view_model_.get(), &RaytracingUserdataViewModel::EnableRayHistoryChanged, capture_ui_->collect_ray_history_checkbox, &QCheckBox::setChecked);
    model_binder_.Connect(capture_ui_->collect_ray_history_checkbox,
                          &QCheckBox::checkStateChanged,
                          view_model.get(),
                          &RaytracingUserdataViewModel::HandleEnableRayHistoryChanged);

    // Marker capture connections
    model_binder_.Connect(
        view_model_.get(), &RaytracingUserdataViewModel::EnableMarkerCaptureChanged, this, &RaytracingUserdataView::OnEnableMarkerCaptureChanged);
    model_binder_.Connect(view_model_.get(), &RaytracingUserdataViewModel::MarkerBeginStringChanged, this, &RaytracingUserdataView::OnMarkerBeginStringChanged);
    model_binder_.Connect(view_model_.get(), &RaytracingUserdataViewModel::MarkerEndStringChanged, this, &RaytracingUserdataView::OnMarkerEndStringChanged);
    model_binder_.Connect(
        enable_marker_capture_checkbox_, &QCheckBox::checkStateChanged, view_model.get(), &RaytracingUserdataViewModel::HandleEnableMarkerCaptureChanged);
    connect(marker_begin_edit_, &QLineEdit::editingFinished, this, &RaytracingUserdataView::OnMarkerBeginEditingFinished);
    connect(marker_end_edit_, &QLineEdit::editingFinished, this, &RaytracingUserdataView::OnMarkerEndEditingFinished);

    // Marker capture support visibility
    model_binder_.Connect(
        view_model_.get(), &RaytracingUserdataViewModel::MarkerCaptureSupportedChanged, this, &RaytracingUserdataView::OnMarkerCaptureSupportedChanged);

    model_binder_.Connect(
        view_model.get(), &RaytracingUserdataViewModel::RayHistoryBufferSizeIndexChanged, this, &RaytracingUserdataView::OnRayHistoryBufferSizeIndexChanged);
    model_binder_.Connect(ray_history_ui_->history_buffer_size,
                          QOverload<int>::of(&QComboBox::currentIndexChanged),
                          this,
                          &RaytracingUserdataView::OnRayHistoryBufferSizeIndexSelectionChanged);

    view_model_->OnBind();
}

void RaytracingUserdataView::OnRayHistoryBufferEditingFinished()
{
    if (view_model_ == nullptr)
    {
        return;
    }

    bool can_convert;
    auto value = ray_history_ui_->history_buffer_size_custom_edit->text().toDouble(&can_convert);
    if (can_convert)
    {
        const uint64_t buffer_size = value * kConversionFactor;
        view_model_->HandleRayHistoryBufferSizeChanged(std::to_string(buffer_size));
    }
}

void RaytracingUserdataView::OnRayHistoryBufferSizedChanged(const std::string& buffer_size)
{
    char*          end;
    const uint64_t buffer_size_num = strtoull(buffer_size.c_str(), &end, 10);
    const double   buffer_size_dbl = static_cast<double>(buffer_size_num) * kConversionFactorDbl;
    ray_history_ui_->history_buffer_size_custom_edit->setText(QString::number(buffer_size_dbl));
}

void RaytracingUserdataView::OnRayHistoryBufferSizeIndexSelectionChanged(int index)
{
    if (view_model_ == nullptr)
    {
        return;
    }

    view_model_->HandleRayHistoryBufferSizeIndexChanged(index);
}

void RaytracingUserdataView::OnRayHistoryBufferSizeIndexChanged(int index)
{
    ray_history_ui_->history_buffer_size->setCurrentIndex(index);
}

void RaytracingUserdataView::OnEnableMarkerCaptureChanged(bool enabled)
{
    enable_marker_capture_checkbox_->setChecked(enabled);
}

void RaytracingUserdataView::OnMarkerBeginStringChanged(const QString& marker_string)
{
    if (!marker_begin_edit_->hasFocus())
    {
        marker_begin_edit_->setText(marker_string);
    }
}

void RaytracingUserdataView::OnMarkerEndStringChanged(const QString& marker_string)
{
    if (!marker_end_edit_->hasFocus())
    {
        marker_end_edit_->setText(marker_string);
    }
}

void RaytracingUserdataView::OnMarkerBeginEditingFinished()
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleMarkerBeginStringChanged(marker_begin_edit_->text());
    }
}

void RaytracingUserdataView::OnMarkerEndEditingFinished()
{
    if (view_model_ != nullptr)
    {
        view_model_->HandleMarkerEndStringChanged(marker_end_edit_->text());
    }
}

void RaytracingUserdataView::OnMarkerCaptureSupportedChanged(bool supported)
{
    if (marker_capture_pane_ != nullptr)
    {
        marker_capture_pane_->setVisible(supported);
    }
}

Ui::RaytracingCapture* RaytracingUserdataView::GetCaptureUi() const
{
    return capture_ui_.get();
}
