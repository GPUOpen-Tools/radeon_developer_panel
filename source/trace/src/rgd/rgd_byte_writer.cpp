// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for RGD byte writer.

#include "rgd_byte_writer.h"

#include "dev_trace_common.h"

namespace devtrace
{
    RgdByteWriter::RgdByteWriter(std::vector<uint8_t>& data)
        : data_(data)
    {
        writer_.pUserdata     = this;
        writer_.pfnBegin      = ByteWriterBegin;
        writer_.pfnWriteBytes = ByteWriterWriteBytes;
        writer_.pfnEnd        = ByteWriterEnd;
    }

    void RgdByteWriter::SetPostProcessOperation([[maybe_unused]] const std::function<void(const std::unique_ptr<ReadWriteStream>&)>& operation)
    {
    }

    const DDByteWriter& RgdByteWriter::Writer()
    {
        return writer_;
    }

    DD_RESULT RgdByteWriter::ByteWriterBegin(void* userdata, const size_t* total_data_size)
    {
        if (total_data_size == nullptr)
        {
            return DD_RESULT_UNKNOWN;
        }

        const auto* writer = static_cast<RgdByteWriter*>(userdata);
        writer->data_.reserve(*total_data_size);

        return DD_RESULT_SUCCESS;
    }

    DD_RESULT RgdByteWriter::ByteWriterWriteBytes(void* userdata, const void* data, const size_t data_size)
    {
        if (data == nullptr || data_size == 0)
        {
            return DD_RESULT_UNKNOWN;
        }

        const auto*  writer       = static_cast<RgdByteWriter*>(userdata);
        const size_t initial_size = writer->data_.size();
        writer->data_.resize(initial_size + data_size);

        memcpy(writer->data_.data() + initial_size, data, data_size);

        return DD_RESULT_SUCCESS;
    }

    void RgdByteWriter::ByteWriterEnd([[maybe_unused]] void* userdata, [[maybe_unused]] DD_RESULT result)
    {
    }
}  // namespace devtrace
