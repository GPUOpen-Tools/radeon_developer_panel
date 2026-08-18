// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for mock file utils.

#ifndef RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_UTILS_H_
#define RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_UTILS_H_

#include <memory>
#include <string>

#include <gmock/gmock.h>

#include <QString>

#include <common/inc/model/file_utils.h>

class MockFileUtils : public FileUtils
{
public:
    ~MockFileUtils() override = default;

    MOCK_METHOD(void, RemoveFile, (const QString&), (const, override));
    MOCK_METHOD(void, CreateFolder, (const QString&), (const, override));
    MOCK_METHOD(bool, VerifyOutputDirectory, (const QString&, bool, bool), (const, override));
};

#endif
