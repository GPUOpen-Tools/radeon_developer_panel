//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <vector>

#include <SDL3/SDL_iostream.h>

class File
{
public:
    explicit File(const char* fileName);
    File(const File& file)            = delete;
    File& operator=(const File& file) = delete;

    ~File();

    [[nodiscard]] std::vector<uint8_t> ReadAll() const;

private:
    SDL_IOStream* m_pStream;
};
