// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for a mock file stream provider.

#ifndef RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_STREAM_PROVIDER_H_
#define RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_STREAM_PROVIDER_H_

#include <memory>
#include <string>

#include <gmock/gmock.h>

#include <QString>

#include <trace_io.h>

#include <common/inc/model/qt_trace_io.h>

class MockFileSystemStreamProvider : public FileSystemStreamProvider
{
public:
    ~MockFileSystemStreamProvider() override = default;

    MOCK_METHOD(void, SetPath, (const QString&), (override));
    MOCK_METHOD(void, SetAppName, (const QString&), (override));
    MOCK_METHOD(std::unique_ptr<devtrace::ReadWriteStream>, CreateReadWriteStream, (std::string&), (override));
    MOCK_METHOD(std::unique_ptr<devtrace::WritableStream>, CreateWritableStream, (std::string&), (override));
    MOCK_METHOD(std::unique_ptr<devtrace::ReadWriteStream>, OpenReadWriteStream, (const std::string&), (override));
};

#endif
