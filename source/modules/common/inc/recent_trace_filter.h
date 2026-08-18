// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Recent trace filter class definition

#ifndef RDP_MODULES_COMMON_RECENT_TRACE_FILTER_H_
#define RDP_MODULES_COMMON_RECENT_TRACE_FILTER_H_

#include <QSortFilterProxyModel>

#include "logger.h"

class RecentTraceFilter : public QSortFilterProxyModel
{
public:
    RecentTraceFilter(QObject* parent = nullptr);

    ~RecentTraceFilter();

    /// @brief Removes the files at the specified indices from the disk and also the model if the deletion was successful.
    /// @param indices The indices of the files to remove.
    /// @param logger The logger to log out to (optional).
    void RemoveFiles(QModelIndexList rows, Logger* logger = nullptr);

protected:
    bool lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const Q_DECL_OVERRIDE;
};

#endif
