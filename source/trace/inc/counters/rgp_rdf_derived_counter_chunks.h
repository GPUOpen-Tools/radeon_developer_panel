// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for RGP dervied counter chunks.

#ifndef RDP_SOURCE_TRACE_INC_RGP_RDF_DERIVED_COUNTER_CHUNKS_H_
#define RDP_SOURCE_TRACE_INC_RGP_RDF_DERIVED_COUNTER_CHUNKS_H_

#include <cstdint>
#include <vector>

#include <amdrdf.h>

#include "rgp_spm_database.h"

static constexpr char kDerivedSpmSessionChunkId[RDF_IDENTIFIER_SIZE] = "DerivedSpmSes";    ///< Derived counter session chunk id.
static constexpr char kDerivedSpmCounterChunkId[RDF_IDENTIFIER_SIZE] = "DerivedSpmCtr";    ///< Derived counter data chunk id.
static constexpr char kDerivedSmpGroupChunkId[RDF_IDENTIFIER_SIZE]   = "DerivedSpmGroup";  ///< Derived counter group chunk id.

///< The chunk header contains configuration data for the trace along with a trailing payload containing the timestamps the SPM counters were sampled at.
struct DerivedSpmSession
{
    uint32_t pci_id;                /// The ID of the GPU the trace ran on.
    uint32_t num_derived_counters;  /// The number of derived SPM counters synthesized.
    uint32_t group_count;           /// The number of groups of derived SPM counters defined.
};

/// @brief Header for a derived counter SPM counter data.
struct DerivedSpmCounterHeader
{
    uint32_t         pci_id;           /// The ID of the GPU the trace ran on.
    uint32_t         counter_index;    /// An index used to identify this counter.
    uint32_t         component_count;  /// The number of component counters used to define this counter.
    CounterUsageType unit;             /// The unit of the values of this counter.
    uint32_t         name_length;      /// The length of the name of this counter.
    uint32_t         desc_length;      /// The length of the description of this counter.
};

/// @brief Header for a derived counter SPM group.
struct DerivedSpmGroupHeader
{
    uint32_t pci_id;        /// The ID of the GPU the trace ran on.
    uint32_t group_index;   /// An index used to identify this group. (do we need this?)
    uint32_t member_count;  /// The number of counters in this group.
    uint32_t name_length;   /// The length of the name of this group.
    uint32_t desc_length;   /// The length of the description of this group.
};

#endif
