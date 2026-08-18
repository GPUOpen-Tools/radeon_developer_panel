// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP API proxy model definition.

#ifndef RDP_SOURCE_FRONTEND_MODELS_API_PROXY_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_API_PROXY_MODEL_H_

#include <memory>

#include <QIdentityProxyModel>

#include "api_model.h"

namespace rdp
{
    /// @brief ApiModel proxy.
    class ApiProxyModel : public QIdentityProxyModel, public std::enable_shared_from_this<ApiProxyModel>
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        /// @param [in] module_model The model that manages the modules.
        explicit ApiProxyModel(const std::shared_ptr<class ModuleModel>& module_model);

        /// @brief Overrides the default QSortFilterProxyModel method, SetSourceApiModel should be used instead as this will throw.
        /// @param [in] source_model Unused.
        void setSourceModel(QAbstractItemModel* source_model) override;

        /// @brief Gets the source model for this proxy.
        /// @return source api model
        ApiModel* GetSourceApiModel() const;

        /// @brief Sets the source model for this proxy.
        /// @param [in] source_model The source model that this proxy should use.
        void SetSourceApiModel(std::shared_ptr<ApiModel> source_model);

        /// @brief Returns the data for the index, adding a tooltip if an item is disabled.
        /// @param [in] index The index to query data for.
        /// @param [in] role The data role.
        /// @return Variant data for specified role.
        QVariant data(const QModelIndex& index, int role) const override;

        /// @brief Returns the flags for the index, making anything unsupported that is not supported by the workflow.
        /// @param [in] index The index of the item to get the flags for.
        /// @return The flags for the item.
        Qt::ItemFlags flags(const QModelIndex& index) const override;

        /// Returns the index for specified API
        /// @param [in] api The API type
        /// @return index of API in model.
        int GetIndexOf(ApiModel::Api api) const;

    signals:
        /// @brief Emitted when the API count of the source model changes.
        /// @param [in] count The new number of available APIs.
        void SourceApiCountChanged(int count);

    private:
        std::shared_ptr<class ModuleModel> module_model_;      ///< The model that manages the modules.
        std::shared_ptr<ApiModel>          source_api_model_;  ///< The source model for this proxy.
    };
}  // namespace rdp

#endif
