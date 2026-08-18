// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for a widget that displays a list of executables.

#include "executable_list_widget.h"

#include <QStringListModel>

#include "ui_executable_list_widget.h"

namespace rdp
{

    ExecutableListWidget::ExecutableListWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::ExecutableListWidget)
    {
        ui_->setupUi(this);
        ui_->label->setStyleSheet("QLabel { border-top: 1px solid palette(mid); }");

        SetShowingTextDescription(false);
    }

    ExecutableListWidget::~ExecutableListWidget() = default;

    void ExecutableListWidget::SetShowingTextDescription(bool show)
    {
        is_showing_text_description_ = show;

        // A stacked widget would be appropriate to use here, however there seems to be some issue with wrapping labels, stacked widgets and scroll areas
        // where there will be extra space. To avoid this, we just stack them in a vertical layout and hide and show them as needed, using a fixed
        // height for the label.
        ui_->label->setVisible(show);
        ui_->list_view->setVisible(!show);
    }

    void ExecutableListWidget::SetTextDescription(const QString& text_desc)
    {
        ui_->label->setText(text_desc);
    }

    void ExecutableListWidget::SetBaseModel(QAbstractItemModel* model)
    {
        ui_->list_view->setModel(model);
    }

    QSize ExecutableListWidget::minimumSizeHint() const
    {
        return sizeHint();
    }
}  // namespace rdp
