// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Implementation for the Auto connect model.

#include "auto_connect_model.h"

#include "models/application_model.h"

namespace rdp
{
    AutoConnectModel::AutoConnectModel(QObject* parent)
        : QAbstractItemModel(parent)
    {
    }

    QVariant AutoConnectModel::data(const QModelIndex& index, const int role) const
    {
        const auto mode = static_cast<ApplicationAutoConnectMode>(index.row());
        if (role == Qt::DisplayRole)
        {
            return GetOptionName(mode);
        }

        if (role == Qt::UserRole)
        {
            return mode;
        }

        return {};
    }

    QString AutoConnectModel::GetOptionName(const ApplicationAutoConnectMode mode)
    {
        switch (mode)
        {
        case kApplicationAutoConnectModeAnyApplication:
            return "Any application";
        case kApplicationAutoConnectModeExistingApplications:
            return "Existing applications";
        case kApplicationAutoConnectModeNoApplications:
            return "None";
        default:
            return "Invalid";
        }
    }

    QModelIndex AutoConnectModel::index(const int row, const int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent))
        {
            return {};
        }

        if (!parent.isValid())
        {
            return createIndex(row, column);
        }

        return {};
    }

    QModelIndex AutoConnectModel::parent(const QModelIndex& index) const
    {
        Q_UNUSED(index)

        return {};
    }

    int AutoConnectModel::rowCount(const QModelIndex& parent) const
    {
        if (!parent.isValid())
        {
            return kApplicationAutoConnectModeNoApplications;
        }

        return 0;
    }

    int AutoConnectModel::columnCount(const QModelIndex& parent) const
    {
        if (!parent.isValid())
        {
            return 1;
        }

        return 0;
    }

}  // namespace rdp
