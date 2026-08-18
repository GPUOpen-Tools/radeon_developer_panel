// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Recent trace model definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_RECENT_TRACE_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_RECENT_TRACE_MODEL_H_

#include <memory>

#include <QAbstractItemModel>
#include <QDateTime>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "model/recent_trace_model_base.h"

/// @brief Provides the data for trace files on disk.
class RecentTraceModel : public QSortFilterProxyModel
{
    Q_OBJECT;

public:
    /// @brief Constructor
    /// @param [in] extension The file extension (rgp, rmt, rra, etc)
    explicit RecentTraceModel(const QString& extension);

    /// @brief Load from output directory
    /// @param [in] outdir The path to output directory
    void Load(const QString& outdir);

    /// @brief Custom sorting implementation for recent trace files
    /// @param [in] left The left comparison index.
    /// @param [in] right The right comparison index.
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const Q_DECL_OVERRIDE;

    /// @brief Gets the path for the file at the index.
    /// @param [in] index The index to get the file path for.
    /// @return The file path for the file at the index.
    QString FilePath(const QModelIndex& index) const;

    /// @brief Removes the file at the given index from the disk.
    /// @param [in] indexes The indexes of the file to remove.
    void Remove(const QList<QModelIndex>& indexes);

signals:
    /// @brief Emitted when a set file name fails
    /// @param [in] message A human-redable message of the error
    void FileNameValidationError(const QString& message);

private:
    std::unique_ptr<class RecentTraceModelBase> base_model_;  ///< The model responsible for providing the actual trace files.
};

#endif
