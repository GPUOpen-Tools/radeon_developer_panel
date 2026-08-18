// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for allocation functions.

#include "api_allocator.h"

#include <cstdlib>

void* ApiAlloc(size_t size)
{
    return std::malloc(size);
}

void* ApiRealloc(void* ptr, size_t size)
{
    return std::realloc(ptr, size);
}

void ApiFree(void* ptr)
{
    std::free(ptr);
}
