// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for ASIC info utilities.

#ifndef RDP_SOURCE_TRACE_INC_ASIC_INFO_H_
#define RDP_SOURCE_TRACE_INC_ASIC_INFO_H_

#include <cstdint>

#include "trace_source.h"

namespace devtrace
{

    /// @brief The different releases of GPUs.
    enum class GpuSeries : uint8_t
    {
        kUnknown = 0,
        kPolaris,    ///< The Polaris series of cards.
        kVega,       ///< The Vega series of cards.
        kNavi1,      ///< The Navi1 series of cards.
        kNavi2,      ///< The Navi1 series of cards.
        kVanGogh,    ///< The Van Gogh series of cards.
        kNavi3,      ///< The Navi3 series of cards.
        kRembrandt,  ///< The Rembrandt series of cards.
        kMgfx,       ///< The MGFX series of cards.
        kPhoenix,    ///< The Phoenix series of cards.
        kRaphael,    ///< The Raphael series of cards.
        kStrix,      ///< The Strix series of cards.
        kKrackan,    ///< The Krackan series of cards.
        kMendocino,  ///< The Mendocino series of cards.
        kNavi4,      ///< The Navi4 series of cards.
    };

    /// @brief The different architectures for GPUs.
    enum class GpuArchitecture : uint8_t
    {
        kUnknown = 0,
        kGcn4,   ///< The GCN4 (Polaris) architecture.
        kGcn5,   ///< The GCN5 (Vega) architecture.
        kRdna1,  ///< The RDNA 1 architecture.
        kRdna2,  ///< The RDNA 2 architecture.
        kRdna3,  ///< The RDNA 3 architecture.
        kRdna4   ///< The RDNA 4 architecture.
    };

    /// @brief An set of utilities that help with determining info about the ASIC.
    struct AsicInfo
    {
        /// @brief Provides the GPU series for an ASIC.
        /// @param [in] device_id The device id.
        /// @param [in] asic_family The family of the ASIC.
        /// @param [in] asic_e_rev The eRevision of the ASIC.
        /// @return The series for the ASIC.
        static GpuSeries GetGpuSeries(uint32_t device_id, uint32_t asic_family, uint32_t asic_e_rev);

        /// @brief Gets the architecture of an ASIC from the GPU series.
        /// @param [in] gpu_series The GPU series to get the architecture for.
        /// @return The architecture of an ASIC from the series.
        static GpuArchitecture GetGpuArchitecture(GpuSeries gpu_series);

        /// @brief Gets the name of the GPU series.
        /// @param [in] gpu_series The GPU series to get the name for.
        /// @return The name the series.
        static std::string GpuSeriesToString(GpuSeries gpu_series);

        /// @brief Gets the GPU series from The name.
        /// @param [in] gpu_series The GPU series name to get the series for.
        /// @return The GPU series.
        static GpuSeries GpuSeriesFromString(const std::string& gpu_series);

    private:
        /// @brief Constructor.
        AsicInfo() = default;
    };

};  // namespace devtrace

#endif
