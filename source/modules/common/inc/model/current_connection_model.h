// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for model that handles current client connections.

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_CURRENT_CONNECTION_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_CURRENT_CONNECTION_MODEL_H_

#include <QAbstractItemModel>
#include <QString>

#include <dev_trace_common.h>

class CurrentConnectionModel final : public QAbstractItemModel
{
    Q_OBJECT
public:
    /// @brief Constructor.
    /// @param [in] parent Parent object for this model.
    explicit CurrentConnectionModel(QObject* parent = nullptr);

    /// @brief Called when the current client connections changes.
    /// @param [in] current_connections The new current client connections.
    void OnCurrentConnectionsChanged(const std::unordered_map<uint16_t, devtrace::Api>& current_connections);

    /// @brief Gets whether or not capture target selection should be disabled.
    /// @return true if capture target selection should be disabled, false otherwise.
    bool IsSelectionEnabled() const;

    /// @brief Gets the connection for the given index.
    /// @param [in] index The index to get the connection id for.
    /// @return The connection id at the given index.
    uint16_t GetConnectionIdForIndex(int index) const;

    /// @brief QAbstractListModel::data() implementation
    /// @param [in] index The index to query data for
    /// @param [in] role The data role
    /// @return variant data for specified role
    QVariant data(const QModelIndex& index, int role) const override;

    /// @brief QAbstractListModel::flags() override
    /// @param [in] row The row to create index for
    /// @param [in] column The column to create index for
    /// @param [in] parent The parent of index
    /// @return new model index
    QModelIndex index(int row, int column, const QModelIndex& parent) const override;

    /// @brief QAbstractListModel::parent() override
    /// @param [in] index The index to return parent for
    /// @return parent index
    QModelIndex parent(const QModelIndex& index) const Q_DECL_OVERRIDE;

    /// @brief QAbstractListModel::rowCount() override
    /// @param [in] parent The parent index
    /// @return number of rows in list
    int rowCount(const QModelIndex& parent) const override;

    /// @brief QAbstractListModel::columnCount() override
    /// @param [in] parent The parent index
    /// @return number of rows in list
    int columnCount(const QModelIndex& parent) const override;

private:
    /// @brief Calculates the index for the capture target.
    /// @param [in] capture_target The capture target.
    void UpdateCaptureTargetIndex(uint32_t capture_target);

    std::vector<std::pair<uint16_t, QString>> sorted_connections_;    ///< All of the current connections and their descriptions.
    bool                                      selection_enabled_;     ///< true if selection should be enabled, false otherwise.
    int32_t                                   capture_target_index_;  ///< The current capture target index.
};

#endif
