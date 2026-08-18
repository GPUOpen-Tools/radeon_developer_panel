// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Api model implementation

#include "api_model.h"

#include "definitions.h"
#include "logging/logging_manager.h"

namespace rdp
{
    ApiModel::ApiModel(const std::weak_ptr<SystemInfoModel>& system_info_model, QObject* parent)
        : QAbstractItemModel(parent)
        , system_info_model_(system_info_model)
    {
        const auto system_info_model_locked = system_info_model_.lock();
        Q_ASSERT(system_info_model_locked != nullptr);

        connect(system_info_model_locked.get(), &SystemInfoModel::SystemInfoChanged, this, &ApiModel::OnSystemInfoChanged);

        Load();
    }

    ApiModel::~ApiModel() = default;

    void ApiModel::Load()
    {
        beginResetModel();

        supported_.clear();

        // Determine number of supported APIs
        supported_.push_back(Api::kWorkflowSupported);

        constexpr auto count = static_cast<int32_t>(Api::kCount);
        for (int i = 0; i < count; i++)
        {
            if (const auto api = static_cast<Api>(i); IsSupported(api))
            {
                supported_.push_back(api);
            }
        }

        // We subtract one here because workflow supported is always enabled, but we want to return the true number of APIs available.
        emit ApiCountChanged(static_cast<int>(supported_.size() - 1));

        endResetModel();
    }

    int ApiModel::GetIndexOf(const Api api)
    {
        if (const auto iter = std::ranges::find(supported_, api); iter != supported_.end())
        {
            return static_cast<int>(std::distance(supported_.begin(), iter));
        }

        return 0;
    }

    void ApiModel::OnSystemInfoChanged()
    {
        // Initialize once we have a new system info loaded
        Load();

        emit Loaded(shared_from_this());
    }

    QString ApiModel::GetName(Api api)
    {
        if (api == Api::kWorkflowSupported)
        {
            return "Any supported";
        }

        return devtrace::GetHumanReadableName(static_cast<devtrace::Api>(api)).c_str();
    }

    ApiModel::Api ApiModel::GetApiFromDriverDescription(const QString& driver_description)
    {
        std::string desc = driver_description.toStdString();
        auto        api  = Api::kUnknown;
        if (driver_description == kAmdOpenCl)
        {
            api = Api::kOpenCl;
        }
        else if (driver_description == kAmdHip)
        {
            api = Api::kHip;
        }
        else if (driver_description == kAmdVulkan)
        {
            api = Api::kVulkan;
        }
        else if (driver_description == kAmdDirectX12)
        {
            api = Api::kDirectX12;
        }
        else if (driver_description == kAmdDirectX9)
        {
            api = Api::kDirectX9;
        }
        else if (driver_description == kAmdDirectX11)
        {
            api = Api::kDirectX11;
        }
        else if (driver_description == kAmdOpenGl)
        {
            api = Api::kOpenGl;
        }

        return api;
    }

    QVariant ApiModel::data(const QModelIndex& index, const int role) const
    {
        const int row = index.row();

        QVariant variant;
        if (role == Qt::DisplayRole)
        {
            const Api api = supported_.at(row);
            variant.setValue(GetName(api));
        }
        else if (role == Qt::UserRole)
        {
            int api = static_cast<int>(supported_.at(row));
            variant.setValue(api);
        }

        return variant;
    }

    int ApiModel::rowCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent)

        return static_cast<int>(supported_.size());
    }

    QVariant ApiModel::headerData(const int section, const Qt::Orientation orientation, const int role) const
    {
        Q_UNUSED(section)
        Q_UNUSED(orientation)
        Q_UNUSED(role)

        return {};
    }

    QModelIndex ApiModel::index(const int row, const int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent))
        {
            return {};
        }

        // Top level
        if (!parent.isValid())
        {
            return createIndex(row, column);
        }

        return {};
    }

    QModelIndex ApiModel::parent(const QModelIndex& index) const
    {
        Q_UNUSED(index)

        return {};
    }

    int ApiModel::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent)

        return 1;
    }

    bool ApiModel::setData(const QModelIndex& index, const QVariant& value, const int role)
    {
        Q_UNUSED(role)
        Q_UNUSED(value)
        Q_UNUSED(role)

        if (!index.isValid())
        {
            return false;
        }

        return false;
    }

    bool ApiModel::insertRows(const int row, const int count, const QModelIndex& parent)
    {
        const int end = row + count - 1;
        beginInsertRows(parent, row, end);

        endInsertRows();

        return true;
    }

    bool ApiModel::removeRows(const int row, const int count, const QModelIndex& parent)
    {
        // Only remove if row is specified
        if (row >= 0)
        {
            const int last = row + (count - 1);
            beginRemoveRows(parent, row, last);

            endRemoveRows();

            return true;
        }

        return false;
    }

    bool ApiModel::IsSupported(const Api api) const
    {
        if (api == Api::kWorkflowSupported)
        {
            return true;
        }

        const auto system_info_model = system_info_model_.lock();
        if (system_info_model == nullptr || !system_info_model->IsValid())
        {
            return false;
        }

        const QString os_name = system_info_model->GetOsName();
        const QString os_desc = system_info_model->GetOsDescription();

        const bool is_windows = os_name.contains(kWindowsIdentifier) || os_desc.contains(kWindowsIdentifier);
        const bool is_linux   = os_name.contains(kLinuxIdentifier) || os_desc.contains(kLinuxIdentifier);
        switch (api)
        {
        case Api::kDirectX12:
        case Api::kOpenCl:
        case Api::kHip:
            return is_windows;

        case Api::kVulkan:
            // Supported on Windows and Linux
            return is_windows || is_linux;

        default:
            return false;
        }
    }
}  // namespace rdp
