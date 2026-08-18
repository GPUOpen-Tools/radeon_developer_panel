// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for model that handles current client connections.

#include "common/inc/model/current_connection_model.h"

#include <algorithm>
#include <string>

#include "model/trace_source_view_model.h"

CurrentConnectionModel::CurrentConnectionModel(QObject* parent)
    : QAbstractItemModel(parent)
    , selection_enabled_(false)
    , capture_target_index_(0)
{
}

void CurrentConnectionModel::OnCurrentConnectionsChanged(const std::unordered_map<uint16_t, devtrace::Api>& current_connections)
{
    beginResetModel();

    std::vector<std::pair<uint16_t, devtrace::Api>> pairs;
    pairs.assign(current_connections.begin(), current_connections.end());

    // Sort first by API and then by connection id
    std::ranges::sort(pairs, [](const auto& lhs, const auto& rhs) {
        if (lhs.second != rhs.second)
        {
            return lhs.second < rhs.second;
        }

        return lhs.first < rhs.first;
    });

    sorted_connections_.clear();
    sorted_connections_.reserve(pairs.size());

    for (const auto& [umd_connection_id, api] : pairs)
    {
        const std::string api_name      = devtrace::GetHumanReadableName(api);
        const int         connection_id = umd_connection_id;

        sorted_connections_.emplace_back(umd_connection_id, QString("%1 (connection %2)").arg(api_name.c_str()).arg(connection_id));
    }

    selection_enabled_ = sorted_connections_.size() > 1;
    endResetModel();
}

bool CurrentConnectionModel::IsSelectionEnabled() const
{
    return selection_enabled_;
}

uint16_t CurrentConnectionModel::GetConnectionIdForIndex(const int index) const
{
    if (index < 0 || index >= static_cast<int>(sorted_connections_.size()))
    {
        return 0;
    }

    return sorted_connections_[index].first;
}

void CurrentConnectionModel::UpdateCaptureTargetIndex(const uint32_t capture_target)
{
    if (sorted_connections_.empty() && capture_target == 0)
    {
        capture_target_index_ = 0;
    }

    for (int index = 0; index < static_cast<int>(sorted_connections_.size()); ++index)
    {
        if (sorted_connections_[index].first == capture_target)
        {
            capture_target_index_ = index;
            return;
        }
    }

    capture_target_index_ = -1;
}

QVariant CurrentConnectionModel::data(const QModelIndex& index, const int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (index.row() < static_cast<int>(sorted_connections_.size()))
    {
        return sorted_connections_[index.row()].second;
    }

    return "None";
}

QModelIndex CurrentConnectionModel::index(const int row, const int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent) || parent.isValid())
    {
        return {};
    }

    return createIndex(row, column);
}

QModelIndex CurrentConnectionModel::parent([[maybe_unused]] const QModelIndex& index) const
{
    return {};
}

int CurrentConnectionModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return qMax(static_cast<int>(sorted_connections_.size()), 1);
}

int CurrentConnectionModel::columnCount([[maybe_unused]] const QModelIndex& parent) const
{
    return 1;
}
