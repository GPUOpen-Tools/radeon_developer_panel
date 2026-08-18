// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation of an individual device clock mode widget object.

#include "clock_mode_widget.h"
#include "device_clocks_gpu_model.h"

#include <QtConcurrent/qtconcurrentrun.h>
#include <QMouseEvent>

#include <qt_common/custom_widgets/message_overlay.h>

#include <common/inc/collapsible_pane.h>

#include "ui_clock_mode_option.h"
#include "ui_clock_mode_widget.h"

ClickableFrame::ClickableFrame(QWidget* parent)
    : QFrame(parent)
{
}

void ClickableFrame::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit Pressed();
        return;
    }

    QFrame::mousePressEvent(event);
}

ClockModeWidget::ClockModeWidget(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::ClockModeWidget)
{
    ui_->setupUi(this);
    ui_->verticalLayout->setAlignment(Qt::AlignTop);

    button_group_.setExclusive(true);
    ui_->unknown_label->setStyleSheet(QString("color: %1").arg(QColor(Qt::darkYellow).name()));
}

ClockModeWidget::~ClockModeWidget() = default;

void ClockModeWidget::SetModel(const std::shared_ptr<DeviceClocksGpuModel>& model)
{
    model_binder_.StartBinding();
    model_binder_.Connect(model.get(), &DeviceClocksGpuModel::ClockModeChanged, this, &ClockModeWidget::OnClockModeChanged);

    model_ = model;
    ui_->gpu_name->setText(model_->GetGpuName());

    RemoveOptions();

    const auto& modes = model_->GetModeModels();
    for (int i = 0; i < static_cast<int>(modes.size()); ++i)
    {
        AddOption(modes[i], i);
    }

    const int  index      = model_->GetCurrentMode();
    const bool is_invalid = index == -1 || index >= static_cast<int>(option_uis_.size());
    ui_->unknown_label->setVisible(is_invalid);

    if (is_invalid)
    {
        if (QAbstractButton* button = button_group_.checkedButton())
        {
            button->setChecked(false);
        }

        return;
    }

    option_uis_[index]->radio_button->setChecked(true);
}

void ClockModeWidget::OnClockModeChanged(const int index) const
{
    const bool is_invalid = index == -1 || index >= static_cast<int>(option_uis_.size());
    ui_->unknown_label->setVisible(is_invalid);

    if (is_invalid)
    {
        if (QAbstractButton* button = button_group_.checkedButton())
        {
            button->setChecked(false);
        }

        return;
    }

    option_uis_[index]->radio_button->setChecked(true);
}

void ClockModeWidget::RemoveOptions()
{
    for (const QList<QAbstractButton*> buttons = button_group_.buttons(); QAbstractButton * button : buttons)
    {
        button_group_.removeButton(button);
    }

    for (QWidget* option : options_)
    {
        ui_->verticalLayout->removeWidget(option);
        delete option;
    }

    option_uis_.clear();
    options_.clear();
}

inline void Style(ClickableFrame* frame, const bool checked)
{
    const QString background = checked ? "highlight" : "midlight";
    const QString sheet      = QString("ClickableFrame { background-color: palette(%1); border: 1px solid palette(dark); border-radius: %2px; }")
                              .arg(background, QString::number(CollapsiblePaneStatics::GetCornerRadius()));

    frame->setBackgroundRole(checked ? QPalette::Highlight : QPalette::Midlight);
    frame->setStyleSheet(sheet);
}

void ClockModeWidget::AddOption(const std::shared_ptr<ClockModeModel>& model, const int index)
{
    const auto mode_widget = new ClickableFrame(this);
    options_.push_back(mode_widget);

    QPalette palette = mode_widget->palette();
    palette.setColor(QPalette::Inactive, QPalette::Highlight, palette.color(QPalette::Active, QPalette::Highlight));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, palette.color(QPalette::Active, QPalette::HighlightedText));
    mode_widget->setPalette(palette);

    Style(mode_widget, false);

    auto mode_ui = std::make_unique<Ui::ClockModeOption>();
    mode_ui->setupUi(mode_widget);

    mode_ui->mode_label->setText(model->GetName());
    mode_ui->mode_label->setPalette(palette);

    mode_ui->desc_label->setText(model->GetDescription());
    mode_ui->desc_label->setPalette(palette);

    mode_ui->shader_clock->setText(QString("<b>Shader clock:</b> %1").arg(model->GetGpuFreqStr()));
    mode_ui->shader_clock->setPalette(palette);

    mode_ui->memory_clock->setText(QString("<b>Memory clock:</b> %1").arg(model->GetMemFreqStr()));
    mode_ui->memory_clock->setPalette(palette);

    connect(mode_widget, &ClickableFrame::Pressed, this, [=, this] { RequestClockMode(index); });
    connect(mode_ui->radio_button, &QRadioButton::pressed, this, [=, this] { RequestClockMode(index); });

    connect(mode_ui->radio_button, &QRadioButton::toggled, this, [=, this](const bool checked) { Style(mode_widget, checked); });

    button_group_.addButton(mode_ui->radio_button);
    option_uis_.push_back(std::move(mode_ui));

    ui_->verticalLayout->addWidget(mode_widget);
}

void ClockModeWidget::RequestClockMode(const int index) const
{
    if (model_ == nullptr)
    {
        return;
    }

    if (const bool result = model_->RequestMode(index); !result)
    {
        MessageOverlay::CriticalAsync("Error",
                                      QString("Failed to update the clock mode for %1").arg(model_->GetGpuName()),
                                      "",
                                      std::function<void(QDialogButtonBox::StandardButton)>(),
                                      QDialogButtonBox::Ok);
    }
}
