// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team

#include "file.hpp"

#include <filesystem>
#include <stdexcept>

#include <fmt/format.h>

#include <SDL3/SDL.h>

namespace
{
    std::string PathForResource(const char* fileName)
    {
        const auto            basePath = SDL_GetBasePath();
        std::filesystem::path path     = std::string(basePath);

        path.append(fileName);
        return path.string();
    }
}  // namespace

File::File(const char* fileName)
{
    const auto path = PathForResource(fileName);
    m_pStream       = SDL_IOFromFile(path.c_str(), "rb");
    if (m_pStream == nullptr)
    {
        throw std::runtime_error(fmt::format("Failed to open {} for read", path));
    }
}

File::~File()
{
    if (m_pStream)
    {
        SDL_CloseIO(m_pStream);
    }
    m_pStream = nullptr;
}

std::vector<uint8_t> File::ReadAll() const
{
    SDL_SeekIO(m_pStream, 0, SDL_IO_SEEK_END);
    const auto numBytes = SDL_TellIO(m_pStream);
    SDL_SeekIO(m_pStream, 0, SDL_IO_SEEK_SET);

    std::vector<uint8_t> bytes(numBytes);

    size_t numBytesRead;
    void*  data = SDL_LoadFile_IO(m_pStream, &numBytesRead, false);
    if (data == nullptr)
    {
        throw std::runtime_error("Failed to read from stream");
    }

    memcpy(bytes.data(), data, numBytesRead);
    SDL_free(data);

    return bytes;
}
