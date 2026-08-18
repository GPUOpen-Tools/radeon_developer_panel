// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief RDF File Model class implementation

#include "model/rdf_file_model.h"
#include "chunk_writing.h"
#include "formatting.h"
#include "model/qt_trace_io.h"

#include <cstdint>

enum RdfFileModelColumn : int32_t
{
    kRdfFileModelColumnIdentifier = 0,
    kRdfFileModelColumnDataSize,
    kRdfFileModelColumnHeaderSize,
    kRdfFileModelColumnVersion,
    kRdfFileModelColumnCount
};

static constexpr int         kColumnCount               = 4;
static constexpr const char* kHeaderNames[kColumnCount] = {"Identifier", "Data Size", "Header Size", "Version"};
static constexpr int         kSpecCount                 = 2;
static constexpr const char* kSpecNames[kSpecCount]     = {"SystemInfo", "RgdExtendedInfo"};

static bool should_read_data(QString chunk_identifier)
{
    if (chunk_identifier.isEmpty())
        return false;
    for (const char* spec_name : kSpecNames)
    {
        if (chunk_identifier.compare(spec_name) == 0)
            return true;
    }
    return false;
}

RdfFileModel::RdfFileModel(const QString& path, QObject* parent)
    : QAbstractTableModel(parent)
{
    int result = rdfResultOk;

    QFileStream stream(path);
    // Read-only open file to fill model
    stream.SetOpenMode(QIODevice::ReadOnly);
    if (!stream.Open())
    {
        error_string_ = QString("Failed to open file for read: %1").arg(path);
        return;
    }

    rdfUserStream user_stream{};
    stream.GetRdfUserStream(user_stream);

    rdfStream* rdf_stream = nullptr;
    result                = rdfStreamCreateFromUserStream(&user_stream, &rdf_stream);
    if (result != rdfResultOk || rdf_stream == nullptr)
    {
        error_string_ = QString("rdfStreamCreateFromUserStream failed (code %1)").arg(result);
        return;
    }

    rdfChunkFile* file = nullptr;
    result             = rdfChunkFileOpenStream(rdf_stream, &file);
    if (result != rdfResultOk || file == nullptr)
    {
        error_string_ = QString("rdfChunkFileOpenStream failed (code %1)").arg(result);
        rdfStreamClose(&rdf_stream);
        return;
    }

    rdfChunkFileIterator* iterator = nullptr;
    result                         = rdfChunkFileCreateChunkIterator(file, &iterator);
    if (result != rdfResultOk || iterator == nullptr)
    {
        error_string_ = QString("rdfChunkFileCreateChunkIterator failed (code %1)").arg(result);
        rdfChunkFileClose(&file);
        rdfStreamClose(&rdf_stream);
        return;
    }

    for (;;)
    {
        int end = 0;
        rdfChunkFileIteratorIsAtEnd(iterator, &end);
        if (end)
        {
            break;
        }

        char     identifier[RDF_IDENTIFIER_SIZE + 1] = {};
        int      index                               = 0;
        int64_t  data_size                           = 0;
        int64_t  header_size                         = 0;
        uint32_t version                             = 0;

        result = rdfChunkFileIteratorGetChunkIdentifier(iterator, identifier);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileIteratorGetChunkIdentifier failed (code %1)").arg(result);
            break;
        }
        result = rdfChunkFileIteratorGetChunkIndex(iterator, &index);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileIteratorGetChunkIndex failed (code %1)").arg(result);
            break;
        }
        result = rdfChunkFileGetChunkDataSize(file, identifier, index, &data_size);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileGetChunkDataSize failed (code %1)").arg(result);
            break;
        }
        result = rdfChunkFileGetChunkHeaderSize(file, identifier, index, &header_size);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileGetChunkHeaderSize failed (code %1)").arg(result);
            break;
        }
        result = rdfChunkFileGetChunkVersion(file, identifier, index, &version);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileGetChunkVersion failed (code %1)").arg(result);
            break;
        }

        uint32_t major_version = 0;
        uint32_t minor_version = 0;
        if (version > static_cast<uint32_t>(std::numeric_limits<short>::max()))
        {
            major_version = version >> 16;
            minor_version = version & 0xFFFF;
        }
        else
        {
            // Assume version number is not packed
            major_version = version;
            minor_version = 0;
        }

        int64_t chunk_count = 0;
        result              = rdfChunkFileGetChunkCount(file, identifier, &chunk_count);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileGetChunkCount failed (code %1)").arg(result);
            break;
        }
        QString chunk_identifier = identifier;
        bool    read_data        = should_read_data(chunk_identifier);
        if (chunk_count > 1)
        {
            chunk_identifier += QString("[%1]").arg(index);
        }

        chunks_.push_back({chunk_identifier, index, data_size, header_size, version, major_version, minor_version});
        if (data_size > 0 && read_data)
        {
            RdfChunk& last_chunk = chunks_.back();
            last_chunk.data.emplace(static_cast<size_t>(data_size));
            std::vector<char>& data_array = *last_chunk.data;
            result                        = rdfChunkFileReadChunkData(file, identifier, index, data_array.data());
            if (result != rdfResultOk)
            {
                error_string_ = QString("rdfChunkFileReadChunkData failed (code %1)").arg(result);
                break;
            }
        }

        result = rdfChunkFileIteratorAdvance(iterator);
        if (result != rdfResultOk)
        {
            error_string_ = QString("rdfChunkFileIteratorAdvance failed (code %1)").arg(result);
            break;
        }
    }

    if (iterator != nullptr)
    {
        int destroy_result = rdfChunkFileDestroyChunkIterator(&iterator);
        if (destroy_result != rdfResultOk && error_string_.isEmpty())
        {
            error_string_ = QString("rdfChunkFileDestroyChunkIterator failed (code %1)").arg(destroy_result);
        }
    }

    int close_result = rdfChunkFileClose(&file);
    if (close_result != rdfResultOk && error_string_.isEmpty())
    {
        error_string_ = QString("rdfChunkFileClose failed (code %1)").arg(close_result);
    }

    rdfStreamClose(&rdf_stream);

    if (error_string_.isEmpty())
    {
        load_ok_ = true;
    }
}

uint64_t RdfFileModel::GetTotalSize() const
{
    uint64_t size = 0;
    for (auto& chunk : chunks_)
    {
        size += chunk.data_size;
    }

    return size;
}

int RdfFileModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid())
    {
        return static_cast<int>(chunks_.size());
    }

    return 0;
}

int RdfFileModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent)

    return kColumnCount;
}

QVariant RdfFileModel::data(const QModelIndex& index, int role) const
{
    auto& chunk = chunks_.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case kRdfFileModelColumnIdentifier:
            return chunk.identifier;
        case kRdfFileModelColumnDataSize:
            return Formatting::FormatBytesPow2(chunk.data_size, 1);
        case kRdfFileModelColumnHeaderSize:
            return Formatting::FormatBytesPow2(chunk.header_size, 1);
        case kRdfFileModelColumnVersion:
            return QString("%1.%2").arg(chunk.major_version).arg(chunk.minor_version);
        case kRdfFileModelColumnCount:
            break;
        }
    }
    else if (role == Qt::UserRole)
    {
        switch (index.column())
        {
        case kRdfFileModelColumnIdentifier:
            return chunk.identifier;
        case kRdfFileModelColumnDataSize:
            return QVariant::fromValue<int64_t>(chunk.data_size);
        case kRdfFileModelColumnHeaderSize:
            return QVariant::fromValue<int64_t>(chunk.header_size);
        case kRdfFileModelColumnVersion:
            return chunk.packed_version;
        case kRdfFileModelColumnCount:
            break;
        }
    }

    return QVariant();
}

QVariant RdfFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        return kHeaderNames[section];
    }

    return QVariant();
}
