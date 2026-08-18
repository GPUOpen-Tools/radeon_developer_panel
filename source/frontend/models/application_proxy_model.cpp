// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Managed Apps Proxy Model class implementation

#include "application_proxy_model.h"

namespace rdp
{
    ApplicationProxyModel::ApplicationProxyModel()
    {
        QSortFilterProxyModel::sort(ApplicationModel::kClientItemColumnName);
    }

    void ApplicationProxyModel::setSourceModel(QAbstractItemModel* source_model)
    {
        Q_UNUSED(source_model)
        qFatal("Use SetApplicationSourceModel instead.");
    }

    void ApplicationProxyModel::SetSourceApplicationModel(std::shared_ptr<ApplicationModel> source_model)
    {
        source_application_model_ = std::move(source_model);
        QSortFilterProxyModel::setSourceModel(source_application_model_.get());
    }

    bool ApplicationProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
    {
        if (left.column() != ApplicationModel::kClientItemColumnName || right.column() != ApplicationModel::kClientItemColumnName || !source_application_model_)
        {
            return QSortFilterProxyModel::lessThan(left, right);
        }

        const QString left_name  = source_application_model_->NameForApplicationAtIndex(left).toLower();
        const QString right_name = source_application_model_->NameForApplicationAtIndex(right).toLower();

        return left_name < right_name;
    }

}  // namespace rdp
