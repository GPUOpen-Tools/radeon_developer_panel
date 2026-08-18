// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Proxy Model class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_RDF_FILE_PROXY_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_RDF_FILE_PROXY_MODEL_H_

#include <QSortFilterProxyModel>

/// @brief Defines proxy model for filtering and sorting RDF chunks
class RdfFileProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    /// @brief Constructor
    /// @param [in] parent The parent object
    explicit RdfFileProxyModel(QObject* parent = nullptr);

    /// @brief Destructor
    virtual ~RdfFileProxyModel() = default;

    /// @brief Checks if specified row should be inserted into table
    /// @param [in] source_row The source row
    /// @param [in] source_parent The source parent index
    /// @return true if inserted, false otherwise
    virtual bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

    /// @brief Compares two model indexes to determine sorting order
    /// @param [in] left The left model index
    /// @param [in] right The right model index
    /// @return true if left index is less than right index
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

#endif
