// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for output path row view.

#include "view/output_path_row_view.h"

#include <QLayout>
#include <QLineEdit>

OutputPathRowView::OutputPathRowView(QWidget* parent)
    : QWidget(parent)
{
}

QSize OutputPathRowView::minimumSizeHint() const
{
    QLineEdit* line_edit = findChild<QLineEdit*>();
    if (line_edit == nullptr)
    {
        return QWidget::minimumSizeHint();
    }

    int required_width = line_edit->minimumSizeHint().width();

    // We need to get a reference to the accessories layout. This is slightly more robust than using findChild and using a hardcoded name
    bool found_accessories_layout = false;
    for (int child = 0; child < layout()->count(); ++child)
    {
        const QWidget* child_widget = layout()->itemAt(child)->widget();
        if (child_widget == line_edit || child_widget == nullptr)
        {
            continue;
        }

        // Even if the buttons are hidden we need to have enough space for them so that when they're unhidden the entire view does not need more space
        const QLayout* child_layout = child_widget->layout();
        for (int access_child = 0; access_child < child_layout->count(); ++access_child)
        {
            required_width += child_layout->itemAt(access_child)->minimumSize().width();
        }

        required_width += child_widget->layout()->spacing() * (child_layout->count() - 1);
        found_accessories_layout = true;

        break;
    }

    Q_ASSERT(found_accessories_layout);
    return {required_width, QWidget::minimumSizeHint().height()};
}
