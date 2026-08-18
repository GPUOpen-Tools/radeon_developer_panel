// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation of a list view that will be the exact size of it's contents.

#include "tight_list_view.h"

#include <qt_common/utils/qt_util.h>

static constexpr int kListBottomMargin = 6;
static constexpr int kMinLines         = 4;

TightListView::TightListView(QWidget* parent)
    : QListView(parent)
{
    setVerticalScrollMode(QAbstractItemView::ScrollMode::ScrollPerPixel);
    setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
    setFrameStyle(QFrame::NoFrame);
}

TightListView::~TightListView() = default;

QSize TightListView::sizeHint() const
{
    if (model() == nullptr)
    {
        return {};
    }

    const int height_from_rows = sizeHintForRow(0) * model()->rowCount();
    return {0, std::max(height_from_rows + kListBottomMargin, GetMinHeight())};
}

QSize TightListView::minimumSizeHint() const
{
    return sizeHint();
}

int TightListView::GetMinHeight() const
{
    ensurePolished();
    return fontMetrics().height() * kMinLines;
}

void TightListView::setModel(QAbstractItemModel* model)
{
    model_binder_.StartBinding();
    QListView::setModel(model);

    model_binder_.Connect(model, &QAbstractItemModel::rowsInserted, this, &TightListView::RowNumberChanged, Qt::DirectConnection);
    model_binder_.Connect(model, &QAbstractItemModel::rowsRemoved, this, &TightListView::RowNumberChanged, Qt::DirectConnection);
    model_binder_.Connect(model, &QAbstractItemModel::modelReset, this, &TightListView::RowNumberChanged, Qt::DirectConnection);

    updateGeometry();
}

void TightListView::RowNumberChanged()
{
    updateGeometry();
}

void TightListView::ScaleFactorChanged()
{
    ensurePolished();

    // Invalidate cached font metrics.
    QtCommon::QtUtils::InvalidateFontMetrics(this);

    updateGeometry();
    update();
}
