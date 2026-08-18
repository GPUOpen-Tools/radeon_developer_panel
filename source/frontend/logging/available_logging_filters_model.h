// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Definitions for debug log filters model.

#ifndef RDP_SOURCE_FRONTEND_LOGGING_AVAILABLE_LOGGING_FILTERS_MODEL_H_
#define RDP_SOURCE_FRONTEND_LOGGING_AVAILABLE_LOGGING_FILTERS_MODEL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <QAbstractItemModel>
#include <QMetaObject>
#include <QString>

#include <common/inc/api/dev_tools_logging.h>
#include "dd_common_api.h"

namespace rdp
{
    /// @brief Model that stores all of the logging items.
    template <typename T>
    class AvailableLoggingFiltersModel : public QAbstractItemModel
    {
    public:
        /// @brief Constructor.
        AvailableLoggingFiltersModel() = default;

        /// @brief Should be called when a message is logged.
        /// @param [in] filterable The property of the message that this model is responsible of keeping track of.
        void MessageLogged(const T& filterable);

        /// @brief Should be called when filter option is selected.
        /// @param [in] new values The collection of new values for selected model.
        void ReplaceValues(const std::set<T>& new_values);

        /// @brief Gets the filter at the given index.
        /// @param [in] index The index of the filter to get.
        /// @param [out] source The source of the filter or empty if the filter is for all sources.
        /// @param [out] pid The PID to filter on or kLoggingInvalidPid if the filter is for all PIDs.
        T GetFilterAtIndex(const QModelIndex& index) const;

        /// @brief Gets the index at the specified row and column with the given parent.
        /// @param [in] row The row of the index.
        /// @param [in] column The column of the index.
        /// @param [in] parent The parent of the index.
        /// @return The index corresponding to the given row, column and parent.
        QModelIndex index(int row, int column, const QModelIndex& parent) const override;

        /// @brief Gets the parent index for the child.
        /// @param [in] child The child to get the parent index for.
        /// @return The parent index for the child.
        QModelIndex parent(const QModelIndex& child) const override;

        /// @brief Returns the number of rows under the given parent.
        /// @param [in] parent The parent to get the number of children for.
        /// @return The number of children for the parent.
        int rowCount(const QModelIndex& parent) const override;

        /// @brief Returns the number of columns under the given parent.
        /// @param [in] parent The parent to get the number of columns for.
        /// @return The number of columns for the parent.
        int columnCount(const QModelIndex& parent) const override;

        /// @brief Gets the data at the specified index for the given role.
        /// @param [in] index The index to get data for.
        /// @param [in] role The role to look at the data with.
        /// @return The data at the given index for the given role.
        QVariant data(const QModelIndex& index, int role) const override;

    private:
        std::mutex unique_values_mutex_;  ///< The lock that guards the filters.

        std::vector<T> sorted_values_;  ///< The sorted values.
        std::set<T>    unique_values_;  ///< All of the unique values.
    };

    using AvailableLoggingFiltersPidModel    = AvailableLoggingFiltersModel<uint32_t>;
    using AvailableLoggingFiltersSourceModel = AvailableLoggingFiltersModel<QString>;
    using AvailableLoggingFiltersUmdIdModel  = AvailableLoggingFiltersModel<DDConnectionId>;

    template <typename T>
    T AvailableLoggingFiltersModel<T>::GetFilterAtIndex(const QModelIndex& index) const
    {
        if (index.row() == 0 || index.parent().isValid())
        {
            return {};
        }

        return sorted_values_[index.row() - 1];
    }

    template <typename T>
    void AvailableLoggingFiltersModel<T>::MessageLogged(const T& filterable)
    {
        {
            std::lock_guard<std::mutex> lock(unique_values_mutex_);
            if (unique_values_.count(filterable) > 0)
            {
                return;
            }

            unique_values_.insert(filterable);
        }

        // We use invoke method so that all updates are on the UI thread which ensures that we can use beginInsertRows how it was intended without causing
        // deadlock from holding a write lock.
        QMetaObject::invokeMethod(this, [filterable, this] {
            int current_size = static_cast<int>(sorted_values_.size());
            for (int insertion_index = 0; insertion_index < current_size; ++insertion_index)
            {
                if (sorted_values_[insertion_index] > filterable)
                {
                    beginInsertRows({}, insertion_index + 1, insertion_index + 1);
                    sorted_values_.insert(sorted_values_.begin() + insertion_index, filterable);
                    endInsertRows();

                    return;
                }
            }

            beginInsertRows({}, current_size + 1, current_size + 1);
            sorted_values_.push_back(filterable);
            endInsertRows();
        });
    }

    template <typename T>
    void AvailableLoggingFiltersModel<T>::ReplaceValues(const std::set<T>& new_values)
    {
        // Full reset on UI thread
        QMetaObject::invokeMethod(this, [this, new_values]() {
            beginResetModel();
            sorted_values_.clear();
            unique_values_.clear();
            // Maintain sorted order
            for (const auto& v : new_values)
            {
                sorted_values_.push_back(v);
                unique_values_.insert(v);
            }
            endResetModel();
        });
    }

    template <typename T>
    QModelIndex AvailableLoggingFiltersModel<T>::index(int row, int column, const QModelIndex& parent) const
    {
        if (!hasIndex(row, column, parent))
        {
            return {};
        }

        return createIndex(row, column);
    }

    template <typename T>
    QModelIndex AvailableLoggingFiltersModel<T>::parent(const QModelIndex& child) const
    {
        Q_UNUSED(child);

        return {};
    }

    template <typename T>
    int AvailableLoggingFiltersModel<T>::rowCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent);
        return 1 + static_cast<int>(sorted_values_.size());
    }

    template <typename T>
    int AvailableLoggingFiltersModel<T>::columnCount(const QModelIndex& parent) const
    {
        Q_UNUSED(parent);
        return 1;
    }

    template <typename T>
    QVariant AvailableLoggingFiltersModel<T>::data(const QModelIndex& index, int role) const
    {
        if (role != Qt::DisplayRole)
        {
            return {};
        }

        const int row = index.row();
        Q_ASSERT(row >= 0);

        if (row == 0)
        {
            return "All";
        }

        Q_ASSERT(row - 1 < static_cast<int>(sorted_values_.size()));
        return sorted_values_[row - 1];
    }

}  // namespace rdp

#endif
