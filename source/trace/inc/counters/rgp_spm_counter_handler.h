// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief Declaration for RGP counter handler.

#ifndef RDP_SOURCE_TRACE_INC_COUNTERS_RGP_SPM_COUNTER_HANDLER_H_
#define RDP_SOURCE_TRACE_INC_COUNTERS_RGP_SPM_COUNTER_HANDLER_H_

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include <dd_gpu_profiling_api.h>
#include <gpu_performance_api/gpu_perf_api_counters.h>
#include <tl/expected.hpp>

#include "dev_trace_common.h"
#include "dipper.h"
#include "logging.h"
#include "spm_db/derived_raw_builder.h"

namespace devtrace
{
    /// @brief The definition for a derived SPM counter.
    struct DerivedSpmCounter
    {
        std::string counter_name;     ///< The counter's GPA name, or user-defined name if using a formula
        std::string counter_formula;  ///< Reverse-polish-notation formula string defining the counter, or empty string if using GPA counter
        std::string display_name;     ///< A user-friendly display name for the counter.
        /// The names of component counters for this counter, along with labels describing their relation to this counter.
        std::vector<spm_db::DerivedFromRawSpmBuilder::Component> components;
    };

    /// @brief A group of derived SPM counters.
    struct DerivedSpmGroup
    {
        std::string              group_name;         ///< The user defined name for the counter group
        std::string              group_description;  ///< Currently unused
        std::vector<std::string> counters;           ///< The names of the counters in the group, as defined by DerivedSpmCounter::counter_name.
    };

    /// @brief The result of an SPM counter query.
    struct SpmCounterQueryResult
    {
        std::vector<DDGpuProfilingSpmCounterId> hardware_counters;  ///< SPM hardware counters to enable.
        std::vector<DerivedSpmCounter>          derived_counters;   ///< derived SPM counters to compute.
        std::vector<DerivedSpmGroup>            derived_groups;     ///< derived SPM counter groups.
    };

    /// @brief An object that is used to handle SPM counters.
    class RgpSpmCounterHandler
    {
    public:
        /// @brief Fills in the hardware counters in a SpmCounterQueryResult based on its derived counters for a given GPU.
        /// This method assumes that SPM is supported for the given GPU.
        /// @param [in] gpu_info The info about the GPU to query the counters for.
        /// @param [in, out] query_result The hardware_counters field of this struct will be set
        ///                               based on the counters defined in its derived_counters field.
        /// @return kSuccess if the query was successful.
        virtual Result QuerySpmCounters(const system_info_utils::GpuInfo& gpu_info, SpmCounterQueryResult& query_result) = 0;

        /// @brief Adds the derived counters for the RGP file at the given path
        /// @param [in] path The path of the file on disk to process (UTF-8 encoded)
        /// @param [in] spm_counters The counters used when the trace was collected.
        /// @param [in] is_rdf true if the file at the given path is an RDF file.
        /// @param [in] progress_callback A callback to report processing progress.
        /// @param [in] should_abort Will be set to true if the processing should be aborted.
        /// @return The result of the processing.
        virtual Result GenerateDerivedCounters(const std::string&                path,
                                               const SpmCounterQueryResult&      spm_counters,
                                               bool                              is_rdf,
                                               const std::function<void(float)>& progress_callback,
                                               const std::atomic_bool&           should_abort) = 0;

        /// @brief Get the default counters and groups for the given hardware.
        /// @param [in] gpu_info A description of the GPU to get default counters for.
        /// @param [out] groups A vector to be filled with the default groups for the given hardware.
        /// @param [out] counters A vector to be filled with the default counters for the given hardware.
        /// @retval
        /// kSuccess on success. The groups and counters vectors will contain the default counters and groups.
        /// @retval
        /// kUnsupported if SPM is not supported on the hardware. The groups and counters vectors will be empty.
        virtual Result DefaultCounters(const system_info_utils::GpuInfo& gpu_info,
                                       std::vector<DerivedSpmGroup>&     groups,
                                       std::vector<DerivedSpmCounter>&   counters) = 0;
    };

    /// @brief GPA implementation of the RgpSpmCounterHandler
    class GpaSpmCounterHandler : public RgpSpmCounterHandler
    {
    public:
        /// @brief Constructor.
        /// @param [in] logger Object used for debug logging.
        DIP(GpaSpmCounterHandler(const std::shared_ptr<class ReadWriteStreamOpener>& stream_opener, const std::shared_ptr<Logger>& logger));

        Result QuerySpmCounters(const system_info_utils::GpuInfo& gpu_info, SpmCounterQueryResult& query_result) override;

    private:
        /// @brief Helper function to add the hardware counters for a specified derived counter.
        ///
        /// This updates the spm_counter_list_, checking for duplicates (so that if more than one.
        /// derived counter uses the same hardware counter, the resulting list will only contain
        /// one instance of the hardware counter.
        ///
        /// @param [in] derived_counter_info The derived counter whose hardware counters should be added
        /// @param [out] query_result The result of the counter query.
        void AddHardwareCountersForDerivedCounter(const GpaDerivedCounterInfo* derived_counter_info, devtrace::SpmCounterQueryResult& query_result);

    public:
        Result GenerateDerivedCounters(const std::string&                path,
                                       const SpmCounterQueryResult&      spm_counters,
                                       bool                              is_rdf,
                                       const std::function<void(float)>& progress_callback,
                                       const std::atomic_bool&           should_abort) override;

        Result DefaultCounters(const system_info_utils::GpuInfo& gpu_info,
                               std::vector<DerivedSpmGroup>&     groups,
                               std::vector<DerivedSpmCounter>&   counters) override;

    private:
        std::shared_ptr<class ReadWriteStreamOpener> stream_opener_;          ///< Object used to open streams for derived counters.
        std::shared_ptr<devtrace::Logger>            logger_;                 ///< Object used for debug logging.
        GpaCounterContext                            gpa_counter_context_{};  ///< The GPA counter context.

        /// @brief Opens a GPA counter context for the specified GPU.
        /// @param [in] target_gpu The target GPU where profiling is being performed.
        /// @return void on success, or an error message string on failure.
        tl::expected<void, std::string> OpenGpaCounterContext(const system_info_utils::GpuInfo& target_gpu);

        /// @brief Closes a previously-opened GPA counter context.
        /// @return true if the GPA counter context was successfully closed, false otherwise.
        bool CloseGpaCounterContext();
    };

}  // namespace devtrace

#endif
