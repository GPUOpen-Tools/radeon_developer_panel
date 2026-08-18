// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP API proxy model implementation.

#include "api_proxy_model.h"

#include <dev_trace_common.h>

#include "models/module_model.h"

namespace rdp
{
    ApiProxyModel::ApiProxyModel(const std::shared_ptr<ModuleModel>& module_model)
        : module_model_(module_model)
    {
    }

    ApiModel* ApiProxyModel::GetSourceApiModel() const
    {
        return source_api_model_.get();
    }

    int ApiProxyModel::GetIndexOf(ApiModel::Api api) const
    {
        return source_api_model_->GetIndexOf(api);
    }

    void ApiProxyModel::setSourceModel(QAbstractItemModel* source_model)
    {
        Q_UNUSED(source_model)
        qFatal("Use SetSourceApiModel instead.");
    }

    void ApiProxyModel::SetSourceApiModel(std::shared_ptr<ApiModel> source_model)
    {
        source_api_model_ = std::move(source_model);
        QIdentityProxyModel::setSourceModel(source_api_model_.get());

        connect(source_api_model_.get(), &ApiModel::ApiCountChanged, this, &ApiProxyModel::SourceApiCountChanged);
    }

    QVariant ApiProxyModel::data(const QModelIndex& index, int role) const
    {
        if (role == Qt::ToolTipRole)
        {
            if ((index.flags() & Qt::ItemIsEnabled) == 0)
            {
                return {"This API is not supported by the currently enabled features."};
            }
        }

        return QIdentityProxyModel::data(index, role);
    }

    Qt::ItemFlags ApiProxyModel::flags(const QModelIndex& index) const
    {
        const Qt::ItemFlags base_flags = QIdentityProxyModel::flags(index);

        const auto api          = static_cast<ApiModel::Api>(index.data(Qt::UserRole).toInt());
        const auto devtrace_api = static_cast<devtrace::Api>(api);

        if (api != ApiModel::Api::kWorkflowSupported && !module_model_->IsApiSupportedByEnabledModules(devtrace_api))
        {
            return base_flags ^ Qt::ItemIsEnabled;
        }

        return base_flags;
    }
};  // namespace rdp
