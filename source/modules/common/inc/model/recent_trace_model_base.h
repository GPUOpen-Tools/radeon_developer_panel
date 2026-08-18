// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Recent trace model base class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_RECENT_TRACE_MODEL_BASE_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_RECENT_TRACE_MODEL_BASE_H_

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QFileSystemWatcher>

/// @brief Base model for displaying recent trace files
class RecentTraceModelBase : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns : uint8_t
    {
        kRecentTraceModelNameColumn = 0,
        kRecentTraceModelSizeColumn,
        kRecentTraceModelDateColumn,
        kRecentTraceModelColumnCount
    };

    /// @brief Constructor
    /// @param [in] extension The file extension for files tracked by this model.
    /// @param [in] parent The parent object.
    explicit RecentTraceModelBase(QString extension, QObject* parent = Q_NULLPTR);

    /// @brief Set the root filesystem path to populate files from.
    /// @param [in] path The filsystem path to populate files from.
    void SetRootPath(const QString& path);

    /// @brief Gets the path for the file at the index.
    /// @param [in] index The index to get the file path for.
    /// @return The file path for the file at the index.
    [[nodiscard]] QString FilePath(const QModelIndex& index) const;

    [[nodiscard]] int rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

    [[nodiscard]] int columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const Q_DECL_OVERRIDE;

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const Q_DECL_OVERRIDE;

    bool removeRows(int row, int count, const QModelIndex& parent) Q_DECL_OVERRIDE;

    bool setData(const QModelIndex& index, const QVariant& value, int role) Q_DECL_OVERRIDE;

private slots:
    /// @brief Handles removal of entry if path have been removed from disk
    /// @param [in] file_path The path to the file.
    void OnFileChanged(const QString& file_path);

    /// @brief Handles response to directory change such as adding a file
    /// @param [in] dir_path
    void OnDirectoryChanged(const QString& dir_path);

signals:
    /// @brief Emitted when a set file name fails
    /// @param [in] message A human-redable message of the error
    void FileNameValidationError(const QString& message);

private:  // NOLINT(*-redundant-access-specifiers)
    /// @brief Loads recent trace files from active root path.
    void Load();

    QString                             extension_;     ///< Extension for recent trace files.
    QString                             root_path_;     ///< Root path to show recent traces for.
    QFileInfoList                       files_;         ///< Files found at root path.
    std::unique_ptr<QFileSystemWatcher> file_watcher_;  ///< Watches for changes on disk to files.
};

#endif
