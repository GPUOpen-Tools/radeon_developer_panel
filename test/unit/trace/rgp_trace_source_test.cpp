// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Tests for the RGP trace source.

#include <memory>

#include <gtest/gtest.h>

#include <ddApi.h>

#include <trace_source_factory.h>

namespace devtrace
{
    TEST(RgpTraceSource, FactoryCreatesNullWithBadDataContext)
    {
        auto factory_result = TraceSourceFactory::CreateRgpSource(static_cast<DDModuleDataContext>(nullptr));
        EXPECT_EQ(factory_result, nullptr);
    }

}  // namespace devtrace
