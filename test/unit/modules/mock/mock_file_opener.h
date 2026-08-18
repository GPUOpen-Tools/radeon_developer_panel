// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Definition for a mock file opener.

#ifndef RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_OPENER_H_
#define RDP_TEST_UNIT_MODULES_MOCK_MOCK_FILE_OPENER_H_

#include <gmock/gmock.h>

#include <QString>

#include <common/inc/model/trace_file_opener.h>

class MockTraceFileOpener : public TraceFileOpener
{
public:
    ~MockTraceFileOpener() override = default;

    MOCK_METHOD(TraceFileOpenerResult, Open, (const QString& file_path, const QString& exe_path), (override));
};

#endif
