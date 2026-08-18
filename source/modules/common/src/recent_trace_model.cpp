// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Recent trace model implementation

#include "recent_trace_model.h"

#include <filesystem>

#include <QDir>
#include <QStringList>
#include <QTextStream>

#include "formatting.h"

RecentTraceModel::RecentTraceModel(const QString& extension)
    : base_model_(new RecentTraceModelBase(extension))
{
    Q_UNUSED(extension)
    QSortFilterProxyModel::setSourceModel(base_model_.get());

    // Propagate signal to parent
    connect(base_model_.get(), &RecentTraceModelBase::FileNameValidationError, this, &RecentTraceModel::FileNameValidationError);
}

bool RecentTraceModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QVariant left_data  = sourceModel()->data(left, Qt::UserRole);
    const QVariant right_data = sourceModel()->data(right, Qt::UserRole);

    const int column = left.column();

    switch (column)
    {
    case RecentTraceModelBase::kRecentTraceModelNameColumn:
    {
        const QString left_str  = left_data.toString();
        const QString right_str = right_data.toString();

        return left_str < right_str;
    }

    case RecentTraceModelBase::kRecentTraceModelSizeColumn:
        return left_data.value<quint64>() < right_data.value<quint64>();

    case RecentTraceModelBase::kRecentTraceModelDateColumn:
        return left_data.toDateTime() < right_data.toDateTime();

    default:
        break;
    }

    return QSortFilterProxyModel::lessThan(left, right);
}

void RecentTraceModel::Load(const QString& outdir)
{
    base_model_->SetRootPath(outdir);
}

QString RecentTraceModel::FilePath(const QModelIndex& index) const
{
    return base_model_->FilePath(mapToSource(index));
}

void RecentTraceModel::Remove(const QList<QModelIndex>& indexes)
{
    auto source_indexes = indexes;
    std::sort(source_indexes.rbegin(), source_indexes.rend());
    for (const auto& index : source_indexes)
    {
        const auto source_index = mapToSource(index);
        base_model_->removeRow(source_index.row());
    }
}
