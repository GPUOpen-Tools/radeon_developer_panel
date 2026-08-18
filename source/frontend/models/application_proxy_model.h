// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDP Managed Apps Proxy Model class definition

#ifndef RDP_SOURCE_FRONTEND_MODELS_APPLICATION_PROXY_MODEL_H_
#define RDP_SOURCE_FRONTEND_MODELS_APPLICATION_PROXY_MODEL_H_

#include <memory>

#include <QSortFilterProxyModel>

#include "application_model.h"
#include "view/model_binder.h"

namespace rdp
{
    /// @brief ApplicationModel proxy
    class ApplicationProxyModel : public QSortFilterProxyModel, public std::enable_shared_from_this<ApplicationProxyModel>
    {
        Q_OBJECT
    public:
        /// @brief Constructor.
        ApplicationProxyModel();

        /// @brief Overrides the default QSortFilterProxyModel method, SetSourceApplicationModel should be used instead
        ///        as this will throw.
        /// @param [in] source_model Unused.
        void setSourceModel(QAbstractItemModel* source_model) override;

        /// @brief Sets the source model for this proxy.
        /// @param [in] source_model The source model that this proxy should use.
        void SetSourceApplicationModel(std::shared_ptr<ApplicationModel> source_model);

    protected:
        /// @brief Comparison function, only works for the name column.
        /// @param [in] left The index on the source model to compare to the right index.
        /// @param [in] right The index on the source model ot compare to the left index.
        /// @return true if the data on the source model at the left index is less than the data at the right index.
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

    private:
        std::shared_ptr<ApplicationModel> source_application_model_;  ///< The source model for this proxy as an ApplicationModel.
    };

}  // namespace rdp

#endif
