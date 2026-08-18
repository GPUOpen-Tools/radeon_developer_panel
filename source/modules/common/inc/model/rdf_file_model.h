// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Model class definition

#ifndef RDP_SOURCE_MODULES_COMMON_INC_MODEL_RDF_FILE_MODEL_H_
#define RDP_SOURCE_MODULES_COMMON_INC_MODEL_RDF_FILE_MODEL_H_

#include <vector>

#include <QAbstractTableModel>
#include <QByteArray>
#include <QString>

#include <amdrdf.h>

/// @brief Describes a single chunk in an rdf file
struct RdfChunk
{
    QString                          identifier;      ///< Chunk idenfitier
    int                              index;           ///< Chunk index for distinguishing subchunks
    int64_t                          data_size;       ///< Chunk data size in bytes
    int64_t                          header_size;     ///< Chunk header size in bytes
    uint32_t                         packed_version;  ///< Chunk packed version
    uint32_t                         major_version;   ///< Chunk major version
    uint32_t                         minor_version;   ///< Chunk minor version
    std::optional<std::vector<char>> data;            ///< (Optional) in-memory chunk data
};

/// @brief  A file model listing RDF file chunks
class RdfFileModel : public QAbstractTableModel
{
public:
    /// @brief Constructor
    /// @param [in] path The rdf file path
    /// @param [in] parent The parent object
    explicit RdfFileModel(const QString& path, QObject* parent = nullptr);

    /// @brief Destructor
    virtual ~RdfFileModel() = default;

    /// @brief Gets the total uncompressed size of the RDF file
    /// @return total uncompressed file size
    uint64_t GetTotalSize() const;

    /// @brief Gets row count for specified index
    /// @param [in] The index to get row count for
    /// @return row count
    virtual int rowCount(const QModelIndex& index) const override;

    /// @brief Gets the column count for index
    /// @param [in] parent The parent index
    /// @return column count
    virtual int columnCount(const QModelIndex& parent) const override;

    /// @brief Gets the data variant for specified index and role
    /// @param [in] index The index to retreive data for
    /// @param [in] role The data role
    /// @return data variant for index
    virtual QVariant data(const QModelIndex& index, int role) const override;

    /// @brief Gets the data variant for header section with role
    /// @param [in] section The header section
    /// @param [in] orientation The orientation
    /// @param [in] role The data role
    /// @return data variant for section
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    /// @brief Whether the file was loaded successfully.
    /// @return true if success, false otherwise
    bool IsLoadOk() const
    {
        return load_ok_;
    }

    /// @brief Error string if loading failed (empty on success).
    /// @return string error with the reason why load failed
    const QString& GetErrorString() const
    {
        return error_string_;
    }

    /// @brief Returns true if the specified row has loaded data.
    bool HasChunkData(int row) const
    {
        if (row < 0 || row >= static_cast<int>(chunks_.size()))
            return false;
        const auto& opt = chunks_[static_cast<size_t>(row)].data;
        return opt && !opt->empty();
    }

    /// @brief Gets the chunk data for the specified row as a QByteArray. Empty if none.
    QByteArray GetChunkData(int row) const
    {
        if (row < 0 || row >= static_cast<int>(chunks_.size()))
            return {};
        const auto& opt = chunks_[static_cast<size_t>(row)].data;
        if (!opt)
            return {};
        // not loaded
        if (opt->empty())
            return {};
        // loaded but empty
        return QByteArray(opt->data(), static_cast<int>(opt->size()));
    }

    /// @brief Gets the identifier for the specified row.
    QString GetChunkIdentifier(int row) const
    {
        if (row < 0 || row >= static_cast<int>(chunks_.size()))
        {
            return QString();
        }
        return chunks_[static_cast<size_t>(row)].identifier;
    }

private:
    std::vector<RdfChunk> chunks_;           ///< Rdf file chunks
    bool                  load_ok_ = false;  ///< True when the file was parsed successfully.
    QString               error_string_;     ///< Non-empty if an error occurred while loading.
};

#endif
