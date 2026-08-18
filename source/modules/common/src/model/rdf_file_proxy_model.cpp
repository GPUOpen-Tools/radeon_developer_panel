// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Proxy Model class implementation

#include "model/rdf_file_proxy_model.h"

RdfFileProxyModel::RdfFileProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

bool RdfFileProxyModel::filterAcceptsRow(int source_row, const QModelIndex& source_parent) const
{
    QModelIndex index0 = sourceModel()->index(source_row, 0, source_parent);
    if (sourceModel()->data(index0, Qt::UserRole).toString().contains(filterRegularExpression()))
    {
        return true;
    }

    return false;
}

bool RdfFileProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    QVariant leftData  = sourceModel()->data(left, Qt::UserRole);
    QVariant rightData = sourceModel()->data(right, Qt::UserRole);

    QMetaType::Type leftType  = (QMetaType::Type)leftData.userType();
    QMetaType::Type rightType = (QMetaType::Type)rightData.userType();

    if (leftType == QMetaType::LongLong && rightType == QMetaType::LongLong)
    {
        return leftData.toULongLong() < rightData.toULongLong();
    }

    return QSortFilterProxyModel::lessThan(left, right);
}
