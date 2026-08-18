// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for the devtrace IO abstraction.

#include "trace_io.h"

namespace
{
    // Rdf user stream
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    int RdfStreamRead(void* userdata, const int64_t count, void* buffer, int64_t* bytes_read)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        return static_cast<int>(!stream->Read(count, buffer, bytes_read));
    }

    int RdfStreamWrite(void* userdata, const int64_t count, const void* buffer, int64_t* bytes_written)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        return static_cast<int>(!stream->Write(count, buffer, bytes_written));
    }

    int RdfStreamTell(void* userdata, int64_t* position)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        return static_cast<int>(!stream->Tell(position));
    }

    int RdfStreamSeek(void* userdata, int64_t position)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        return static_cast<int>(!stream->Seek(position));
    }

    int RdfStreamGetSize(void* userdata, int64_t* size)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        return static_cast<int>(!stream->GetSize(size));
    }

    int RdfStreamClose(void* userdata)
    {
        devtrace::ReadWriteStream* stream = reinterpret_cast<devtrace::ReadWriteStream*>(userdata);
        stream->Close();

        return 0;
    }

}  // namespace

namespace devtrace
{
    std::unique_ptr<WritableStream> devtrace::ReadWriteStreamProvider::CreateWritableStream(std::string& path)
    {
        return CreateReadWriteStream(path);
    }

    void ReadWriteStream::GetRdfUserStream(rdfUserStream& stream)
    {
        stream.Read  = &RdfStreamRead;
        stream.Write = &RdfStreamWrite;

        stream.Tell    = &RdfStreamTell;
        stream.Seek    = &RdfStreamSeek;
        stream.GetSize = &RdfStreamGetSize;
        stream.Close   = &RdfStreamClose;

        stream.context = this;
    }

}  // namespace devtrace
