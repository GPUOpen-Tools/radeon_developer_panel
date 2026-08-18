// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for RGD byte writer.

#ifndef RDP_SOURCE_TRACE_SRC_RGD_BYTE_WRITER_H_
#define RDP_SOURCE_TRACE_SRC_RGD_BYTE_WRITER_H_

#include <functional>
#include <vector>

#include "../base_trace_source/file_writing.h"

#include "trace_io.h"

namespace devtrace
{
    /// @brief Special ByteWriter for RGD that writes to memory.
    class RgdByteWriter final : public ByteWriter
    {
    public:
        /// @brief Constructor.
        /// @param [in] data  The place to write data to.
        explicit RgdByteWriter(std::vector<uint8_t>& data);

        void                SetPostProcessOperation(const std::function<void(const std::unique_ptr<ReadWriteStream>&)>& operation) override;
        const DDByteWriter& Writer() override;

    private:
        /// @brief Callback for the begin call for a DDByteWriter.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] total_data_size The total size of the data about to be dumped.
        /// @return DD_RESULT_SUCCESS
        static DD_RESULT ByteWriterBegin(void* userdata, const size_t* total_data_size);

        /// @brief Writes some data to the currently dumping trace file.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] data The data to write.
        /// @param [in] data_size The number of bytes of data to write.
        /// @return DD_RESULT_SUCCESS if writing is successful, otherwise DD_RESULT_UNKNOWN.
        static DD_RESULT ByteWriterWriteBytes(void* userdata, const void* data, size_t data_size);

        /// @brief Ends writing a trace file.
        /// @param [in] userdata The instance of BaseTraceSource the callback is for.
        /// @param [in] result The final result of the transfer.
        static void ByteWriterEnd(void* userdata, DD_RESULT result);

        std::vector<uint8_t>& data_;      ///< The place to write data to.
        DDByteWriter          writer_{};  ///< The byte writer.
    };

}  // namespace devtrace
#endif
