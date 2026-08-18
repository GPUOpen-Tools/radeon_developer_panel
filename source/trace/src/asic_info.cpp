// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Implementation for ASIC info utilities.

#include "asic_info.h"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace devtrace
{
    // See amdgpu_asic.h and Device::DetermineGpuIpLevels() in PAL
    static constexpr uint32_t kFamilyPolaris = 0x82;

    static constexpr uint32_t kGfx9Start = 0x8D;
    static constexpr uint32_t kGfx9End   = 0x8E;

    static constexpr uint32_t kFamilyNavi            = 0x8F;
    static constexpr uint32_t kNavi2XMinimumRevision = 0x28;

    static constexpr uint32_t kFamilyVanGogh   = 0x90;
    static constexpr uint32_t kFamilyNavi3     = 0x91;
    static constexpr uint32_t kFamilyRembrandt = 0x92;
    static constexpr uint32_t kFamilyMgfx      = 0x93;
    static constexpr uint32_t kFamilyPhoenix   = 0x94;
    static constexpr uint32_t kFamilyRaphael   = 0x95;
    static constexpr uint32_t kFamilyStrix     = 0x96;
    static constexpr uint32_t kFamilyMendocino = 0x97;
    static constexpr uint32_t kFamilyNavi4     = 0x98;

    static constexpr uint32_t kStrix1Id    = 0x150E;
    static constexpr uint32_t kStrixHaloId = 0x1586;
    static constexpr uint32_t kKrackan1Id  = 0x1114;
    static constexpr uint32_t kKrackan2Id  = 0x1902;

    static const std::array<uint32_t, 2> kKrackanDevices = {kKrackan1Id, kKrackan2Id};

    static const std::unordered_map<GpuSeries, std::string> kGpuSeriesNames = {{GpuSeries::kPolaris, "Polaris"},
                                                                               {GpuSeries::kVega, "Vega"},
                                                                               {GpuSeries::kNavi1, "Navi 1"},
                                                                               {GpuSeries::kNavi2, "Navi 2"},
                                                                               {GpuSeries::kVanGogh, "Van Gogh"},
                                                                               {GpuSeries::kNavi3, "Navi 3"},
                                                                               {GpuSeries::kRembrandt, "Rembrandt"},
                                                                               {GpuSeries::kMgfx, "MGFX"},
                                                                               {GpuSeries::kPhoenix, "Phoenix"},
                                                                               {GpuSeries::kRaphael, "Raphael"},
                                                                               {GpuSeries::kStrix, "Strix"},
                                                                               {GpuSeries::kMendocino, "Mendocino"},
                                                                               {GpuSeries::kNavi4, "Navi 4"}};

    GpuSeries devtrace::AsicInfo::GetGpuSeries(const uint32_t device_id, const uint32_t asic_family, const uint32_t asic_e_rev)
    {
        if (std::ranges::find(kKrackanDevices, device_id) != kKrackanDevices.end())
        {
            return GpuSeries::kKrackan;
        }

        if (asic_family == kFamilyPolaris)
        {
            return GpuSeries::kPolaris;
        }

        if (asic_family >= kGfx9Start && asic_family <= kGfx9End)
        {
            return GpuSeries::kVega;
        }

        if (asic_family == kFamilyNavi)
        {
            return asic_e_rev < kNavi2XMinimumRevision ? GpuSeries::kNavi1 : GpuSeries::kNavi2;
        }

        // This is derived from Gfx9::DetermineIpLevel() in PAL
        switch (asic_family)
        {
        case kFamilyVanGogh:
            return GpuSeries::kVanGogh;
        case kFamilyRembrandt:
            return GpuSeries::kRembrandt;
        case kFamilyMgfx:
            return GpuSeries::kMgfx;
        case kFamilyRaphael:
            return GpuSeries::kRaphael;
        case kFamilyMendocino:
            return GpuSeries::kMendocino;
        case kFamilyNavi3:
            return GpuSeries::kNavi3;
        case kFamilyPhoenix:
            return GpuSeries::kPhoenix;
        case kFamilyStrix:
            return GpuSeries::kStrix;
        case kFamilyNavi4:
            return GpuSeries::kNavi4;

        default:
            return GpuSeries::kUnknown;
        }
    }

    GpuArchitecture AsicInfo::GetGpuArchitecture(GpuSeries gpu_series)
    {
        switch (gpu_series)
        {
        case GpuSeries::kPolaris:
            return GpuArchitecture::kGcn4;
        case GpuSeries::kVega:
            return GpuArchitecture::kGcn5;
        case GpuSeries::kNavi1:
            return GpuArchitecture::kRdna1;
        case GpuSeries::kNavi2:
        case GpuSeries::kVanGogh:
        case GpuSeries::kRembrandt:
        case GpuSeries::kMgfx:
        case GpuSeries::kRaphael:
        case GpuSeries::kMendocino:
            return GpuArchitecture::kRdna2;
        case GpuSeries::kNavi3:
        case GpuSeries::kPhoenix:
        case GpuSeries::kStrix:
        case GpuSeries::kKrackan:
            return GpuArchitecture::kRdna3;
        case GpuSeries::kNavi4:
            return GpuArchitecture::kRdna4;
        default:
            return GpuArchitecture::kUnknown;
        }
    }

    std::string AsicInfo::GpuSeriesToString(GpuSeries gpu_series)
    {
        if (kGpuSeriesNames.count(gpu_series) > 0)
        {
            return kGpuSeriesNames.at(gpu_series);
        }

        return "Unknown";
    }

    GpuSeries AsicInfo::GpuSeriesFromString(const std::string& gpu_series)
    {
        for (const auto& pair : kGpuSeriesNames)
        {
            if (pair.second == gpu_series)
            {
                return pair.first;
            }
        }

        return GpuSeries::kUnknown;
    }
};  // namespace devtrace
