// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Device clocks view implementation

#include "device_clocks_view.h"

#include <QScrollBar>
#include <QThread>
#include <memory>

#include "clock_mode_widget.h"
#include "ui_device_clocks_view.h"

static constexpr int kMaxScrollAreaSizeHintChildren = 2;
static constexpr int kScrollAreaMargins             = 24;

DeviceClocksScrollArea::DeviceClocksScrollArea(QWidget* parent)
    : QScrollArea(parent)
{
    setViewportMargins(kScrollAreaMargins, 0, kScrollAreaMargins, 0);
}

QSize DeviceClocksScrollArea::sizeHint() const
{
    return minimumSizeHint();
}

QSize DeviceClocksScrollArea::minimumSizeHint() const
{
    const QSize    super_size_hint = QScrollArea::minimumSizeHint();
    const QWidget* contents        = widget();
    if (contents == nullptr || contents->layout() == nullptr)
    {
        return super_size_hint;
    }

    const int children   = std::min(contents->layout()->count(), kMaxScrollAreaSizeHintChildren);
    int       width_hint = (children - 1) * contents->layout()->spacing() + kScrollAreaMargins * 2;

    for (int i = 0; std::cmp_less(i, children); ++i)
    {
        width_hint += contents->layout()->itemAt(i)->minimumSize().width();
    }

    return {width_hint, contents->minimumSizeHint().height() + horizontalScrollBar()->height()};
}

DeviceClocksView::DeviceClocksView(QWidget* parent)
    : QWidget(parent)
    , ui_(new Ui::DeviceClocksView)
{
    ui_->setupUi(this);

    QPalette warning_palette = ui_->profiling_warning->palette();
    warning_palette.setColor(QPalette::WindowText, Qt::darkYellow);
    ui_->profiling_warning->setPalette(warning_palette);

    ui_->headerLayout->setAlignment(Qt::AlignTop);
}

DeviceClocksView::~DeviceClocksView() = default;

void DeviceClocksView::SetModel(const std::shared_ptr<DeviceClocksModel>& model)
{
    model_ = model;

    connect(model_.get(), &DeviceClocksModel::GpusChanged, this, &DeviceClocksView::OnGpusChanged);
}

void DeviceClocksView::RemoveButtons()
{
    for (auto& widget : clock_mode_widgets_)
    {
        ui_->horizontalLayout->removeWidget(widget.get());
        widget->hide();
    }

    ui_->button_area->updateGeometry();
}

void DeviceClocksView::OnGpusChanged(QVector<std::shared_ptr<DeviceClocksGpuModel>> gpus, const bool is_connected)
{
    RemoveButtons();

    if (!is_connected)
    {
        ui_->warning_label->setText("Connect to a system to begin using device clocks.");
        ui_->stacked_widget->setCurrentWidget(ui_->warning_page);
        return;
    }

    if (gpus.empty())
    {
        ui_->warning_label->setText("The system does not have any GPUs that can be used with device clocks.");
        ui_->stacked_widget->setCurrentWidget(ui_->warning_page);
        return;
    }

    // Step through each clock mode we want to query and add a widget for it.
    for (int i = 0; i < gpus.count(); ++i)
    {
        // If there are not enough widgets allocated, allocate one more for this mode
        if (i >= clock_mode_widgets_.size())
        {
            clock_mode_widgets_.append(std::make_shared<ClockModeWidget>(this));
        }

        const std::shared_ptr mode_widget(clock_mode_widgets_.at(i));
        mode_widget->SetModel(gpus[i]);

        ui_->horizontalLayout->addWidget(mode_widget.get());

        mode_widget->show();
        mode_widget->setEnabled(true);
    }

    ui_->button_area->updateGeometry();
    ui_->stacked_widget->setCurrentWidget(ui_->button_page);
}
