// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for allocation functions.

#ifndef RDP_SOURCE_API_CAPTURE_API_ALLOCATOR_H_
#define RDP_SOURCE_API_CAPTURE_API_ALLOCATOR_H_

#include <cstddef>
#include <cstdint>

/// @brief Allocates a block of memory.
/// @param [in] size The size of the memory to allocate.
/// @return The allocated memory.
void* ApiAlloc(size_t size);

/// @brief Reallocates a block of memory.
/// @param [in] ptr The block of memory to reallocate.
/// @param [in] size The new of the memory block.
/// @return The allocated memory.
void* ApiRealloc(void* ptr, size_t size);

/// @brief Frees a block of allocated memory.
/// @param [in] ptr The pointer to free.
void ApiFree(void* ptr);

#endif
