// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Definition for the Auto connect model.

#ifndef RDP_SOURCE_FRONTEND_MODELS_SIDEBAR_AUTO_CONNECT_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_SIDEBAR_AUTO_CONNECT_MODEL_H_

#include <memory>

#include "models/application_model.h"

namespace rdp
{
    /// @brief Model for displaying the different auto connect modes.
    class AutoConnectModel final : public QAbstractItemModel, public std::enable_shared_from_this<AutoConnectModel>
    {
        Q_OBJECT

    public:
        /// @brief Constructor.
        /// @param [in] parent The parent object for this model.
        explicit AutoConnectModel(QObject* parent = nullptr);

        /// @brief QAbstractListModel::data() implementation
        /// @param [in] index The index to query data for
        /// @param [in] role The data role
        /// @return variant data for specified role
        QVariant data(const QModelIndex& index, int role) const override;

    private:
        /// @brief Gets the name of the auto connection mode.
        /// @param [in] mode The mode to get the name of.
        /// @return The name of the auto connection mode.
        static QString GetOptionName(ApplicationAutoConnectMode mode);

    public:
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
    };

}  // namespace rdp

#endif
