// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools
/// @file
/// @brief  Implementation for the GPA SPM counter handler.

#include "counters/rgp_spm_counter_handler.h"

#include <algorithm>
#include <array>
#include <sstream>

#include <spm_db/gpa_counters_loader.h>

#include "asic_info.h"
#include "counters/rgp_derived_spm_database.h"
#include "logging_definitions.h"
#include "trace_io.h"

namespace devtrace
{
    GpaSpmCounterHandler::GpaSpmCounterHandler(const std::shared_ptr<ReadWriteStreamOpener>& stream_opener, const std::shared_ptr<devtrace::Logger>& logger)
        : stream_opener_(stream_opener)
        , logger_(logger->WithSource("SPM Counter Handler"))
    {
    }

    Result GpaSpmCounterHandler::QuerySpmCounters(const system_info_utils::GpuInfo& gpu_info, SpmCounterQueryResult& query_result)
    {
        query_result.hardware_counters.clear();

        auto open_result = OpenGpaCounterContext(gpu_info);
        if (!open_result)
        {
            logger_->LogError(open_result.error().c_str(), kLoggingInvalidPid, kLoggingInvalidUmdId);
            return devtrace::Result::kFailure;
        }

        const GpaCounterLibFuncTable* gpa_func_table = GPUPerfAPICountersEntryPoints::Instance()->GetFuncTable();
        DEV_TRACE_ASSERT(gpa_func_table != nullptr);
        if (gpa_counter_context_ == nullptr)
        {
            return devtrace::Result::kFailure;
        }

        // Add hardware counters for each derived counter.
        for (const devtrace::DerivedSpmCounter& requested_counter : query_result.derived_counters)
        {
            if (!requested_counter.counter_formula.empty())
            {
                // If the counter formula is not empty, then this is a formula counter and not a GPA counter, so we shouldn't try to query it in GPA.
                // Formula counters have their required counters added to the requested derived counter list when we process their formulas,
                // (see ProfilingViewModel::ParseSpmCounterFile) which we process here separately from the formula counter,
                // so we don't need to process the formula counters themselves here.
                continue;
            }

            const std::string& counter_name = requested_counter.counter_name;

            GpaCounterParam counter_param;
            counter_param.is_derived_counter   = true;  // by name, not by block, instance, event
            counter_param.derived_counter_name = counter_name.c_str();

            GpaUInt32 counter_index;
            GpaStatus gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(gpa_counter_context_, &counter_param, &counter_index);
            if (gpa_status != kGpaStatusOk)
            {
                // Custom counters may name hardware counters without a block index.
                // If we didn't find a counter, try adding a 0 before the first or second underscore
                // as a block index to make a valid hardware counter name.
                for (size_t upos = counter_name.find('_'); upos != std::string::npos; upos = counter_name.find('_', upos + 1))
                {
                    const std::string counter_name_zero = counter_name.substr(0, upos) + "0" + counter_name.substr(upos);

                    counter_param.derived_counter_name = counter_name_zero.c_str();

                    gpa_status = gpa_func_table->GpaCounterLibGetCounterIndex(gpa_counter_context_, &counter_param, &counter_index);
                    if (kGpaStatusOk == gpa_status)
                    {
                        break;
                    }
                }
            }

            DEV_TRACE_ASSERT(kGpaStatusOk == gpa_status);
            if (kGpaStatusOk == gpa_status)
            {
                const GpaCounterInfo* counter_info;
                gpa_status = gpa_func_table->GpaCounterLibGetCounterInfo(gpa_counter_context_, counter_index, &counter_info);
                DEV_TRACE_ASSERT(kGpaStatusOk == gpa_status);
                if (kGpaStatusOk == gpa_status)
                {
                    DEV_TRACE_ASSERT(nullptr != counter_info);

                    if (counter_info->is_derived_counter)
                    {
                        AddHardwareCountersForDerivedCounter(counter_info->gpa_derived_counter, query_result);
                    }
                    else
                    {
                        query_result.hardware_counters.push_back(
                            {static_cast<uint32_t>(counter_info->gpa_hw_counter->gpa_hw_block), 4095, counter_info->gpa_hw_counter->gpa_hw_block_event_id});
                    }
                }
            }
        }

        if (!CloseGpaCounterContext())
        {
            logger_->LogError("Error when closing GPA counter context", kLoggingInvalidPid, kLoggingInvalidUmdId);
        }

        return devtrace::Result::kSuccess;
    }

    void GpaSpmCounterHandler::AddHardwareCountersForDerivedCounter(const GpaDerivedCounterInfo* derived_counter_info, SpmCounterQueryResult& query_result)
    {
        GpaHwBlock prev_hw_block = kGpaHwBlockCount;
        GpaUInt32  prev_event_id = static_cast<GpaUInt32>(-1);
        DEV_TRACE_ASSERT(0 < derived_counter_info->gpa_hw_counter_count);

        for (GpaUInt32 j = 0; j < derived_counter_info->gpa_hw_counter_count; j++)
        {
            const GpaHwCounter hw_counter = derived_counter_info->gpa_hw_counters[j];
            // if a counter is a timing counter, ignore it -- RGP will fill in the timing value when calculating the derived counter value
            if (!hw_counter.is_timing_block && (prev_hw_block != hw_counter.gpa_hw_block || prev_event_id != hw_counter.gpa_hw_block_event_id))
            {
                // Check if the current hardware counter is already enabled. If so, skip it (don't add it to the list again).
                const bool hw_counter_found = std::ranges::any_of(query_result.hardware_counters, [&](const auto& c) {
                    return c.blockId == static_cast<uint32_t>(hw_counter.gpa_hw_block) && c.eventId == hw_counter.gpa_hw_block_event_id;
                });

                if (!hw_counter_found)
                {
                    query_result.hardware_counters.push_back({static_cast<uint32_t>(hw_counter.gpa_hw_block), 4095, hw_counter.gpa_hw_block_event_id});
                }
                prev_hw_block = hw_counter.gpa_hw_block;
                prev_event_id = hw_counter.gpa_hw_block_event_id;
            }
        }
    }

    Result GpaSpmCounterHandler::GenerateDerivedCounters(const std::string&                path,
                                                         const SpmCounterQueryResult&      spm_counters,
                                                         bool                              is_rdf,
                                                         const std::function<void(float)>& progress_callback,
                                                         const std::atomic_bool&           should_abort)
    {
        DerivedSpmDbArguments args;
        args.stream = stream_opener_->OpenReadWriteStream(path);
        args.is_rdf = is_rdf;

        args.counters = spm_counters.derived_counters;
        args.groups   = spm_counters.derived_groups;

        args.should_abort      = &should_abort;
        args.progress_callback = progress_callback;

        args.logger = logger_;

        Result result = AppendChunkDerivedSpmDb(args) == kRgpFileWriterStatusOk ? devtrace::Result::kSuccess : devtrace::Result::kFailure;
        args.stream->Close();

        return result;
    }
    Result GpaSpmCounterHandler::DefaultCounters(const system_info_utils::GpuInfo& gpu_info,
                                                 std::vector<DerivedSpmGroup>&     groups,
                                                 std::vector<DerivedSpmCounter>&   counters)
    {
        const uint32_t& family     = gpu_info.asic.id_info.family;
        const uint32_t& e_revision = gpu_info.asic.id_info.e_rev;
        const uint32_t& device     = gpu_info.asic.id_info.device;

        groups.clear();
        counters.clear();
        devtrace::GpuArchitecture arch = devtrace::AsicInfo::GetGpuArchitecture(devtrace::AsicInfo::GetGpuSeries(device, family, e_revision));
        if (arch < devtrace::GpuArchitecture::kRdna1)
        {
            return Result::kUnsupported;
        }

        // Cache counters
        constexpr std::array cache_levels = {"Inst", "Scalar", "L0", "L1", "L2"};
        for (const char* cache_lv_chars : cache_levels)
        {
            std::string cache_lv(cache_lv_chars);
            // There are no L1 cache counters on Navi4. Do not add them to the list of counters.
            if (arch == devtrace::GpuArchitecture::kRdna4 && cache_lv == "L1")
            {
                continue;
            }
            std::string friendly_cache_lv = (cache_lv == "Inst") ? "Instruction" : cache_lv;
            std::string hit_pct_name      = cache_lv + "CacheHit";
            std::string req_cnt_name      = cache_lv + "CacheRequestCount";
            std::string hit_cnt_name      = cache_lv + "CacheHitCount";
            std::string miss_cnt_name     = cache_lv + "CacheMissCount";
            counters.push_back(devtrace::DerivedSpmCounter{
                hit_pct_name, "", friendly_cache_lv + " cache hit", {{"Requests", req_cnt_name}, {"Hits", hit_cnt_name}, {"Misses", miss_cnt_name}}});

            // TODO set friendly names correctly once we have support in the chunk or otherwise resolve the issue.
            counters.push_back(devtrace::DerivedSpmCounter{req_cnt_name, "", "Requests", {}});  // display name: friendly_cache_lv + " cache requests"
            counters.push_back(devtrace::DerivedSpmCounter{hit_cnt_name, "", "Hits", {}});      // display name: friendly_cache_lv + " cache hits"
            counters.push_back(devtrace::DerivedSpmCounter{miss_cnt_name, "", "Misses", {}});   // display name: friendly_cache_lv + " cache misses"
        }

        // There are no L1 cache counters on Navi4. Do not include L1CacheHit in the cache counter group.
        if (arch == devtrace::GpuArchitecture::kRdna4)
        {
            groups.push_back({"Cache", "", {"InstCacheHit", "ScalarCacheHit", "L0CacheHit", "L2CacheHit"}});
        }
        else
        {
            groups.push_back({"Cache", "", {"InstCacheHit", "ScalarCacheHit", "L0CacheHit", "L1CacheHit", "L2CacheHit"}});
        }

        // Raytracing counters
        counters.push_back(devtrace::DerivedSpmCounter{"RayBoxTests", "", "Ray-box tests", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"RayTriTests", "", "Ray-triangle tests", {}});
        groups.push_back(devtrace::DerivedSpmGroup{"Ray tracing", "", {"RayBoxTests", "RayTriTests"}});

        // LDS and memory counters are only supported on RDNA3 or higher. For older hardware, we are done.
        if (arch < devtrace::GpuArchitecture::kRdna3)
        {
            return Result::kSuccess;
        }

        // LDS counters
        counters.push_back(devtrace::DerivedSpmCounter{
            "CSLDSBankConflict", "", "LDS Bank Conflict", {{"GPU Busy Cycles", "GPUBusyCycles"}, {"LDS Busy Cycles", "CSLDSBankConflictCycles"}}});
        counters.push_back(devtrace::DerivedSpmCounter{"GPUBusyCycles", "", "Gpu Busy Cycles", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"CSLDSBankConflictCycles", "", "LDS Busy Cycles", {}});
        groups.push_back(devtrace::DerivedSpmGroup{"LDS", "", {"CSLDSBankConflict"}});

        // Memory (percent) counters
        counters.push_back(devtrace::DerivedSpmCounter{
            "MemUnitBusy", "", "Memory unit busy", {{"GPU Busy Cycles", "GPUBusyCycles"}, {"Mem Unit Busy Cycles", "MemUnitBusyCycles"}}});
        counters.push_back(devtrace::DerivedSpmCounter{
            "MemUnitStalled", "", "Memory unit stalled", {{"GPU Busy Cycles", "GPUBusyCycles"}, {"Mem Unit Stalled Cycles", "MemUnitStalledCycles"}}});
        counters.push_back(devtrace::DerivedSpmCounter{"MemUnitBusyCycles", "", "Memory unit busy cycles", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"MemUnitStalledCycles", "", "Memory unit stalled cycles", {}});

        // RDNA 3 GPUs do not support enough simultaneous hardware counters to capture all of the memory counters we want,
        // so we leave off WriteUnitStalled to remain under the limit.
        if (arch >= devtrace::GpuArchitecture::kRdna4)
        {
            counters.push_back(devtrace::DerivedSpmCounter{
                "WriteUnitStalled", "", "Write unit stalled", {{"GPU Busy Cycles", "GPUBusyCycles"}, {"Write Unit Stalled Cycles", "WriteUnitStalledCycles"}}});
            counters.push_back(devtrace::DerivedSpmCounter{"WriteUnitStalledCycles", "", "Write unit stalled cycles", {}});
            groups.push_back(devtrace::DerivedSpmGroup{"Memory (%)", "", {"MemUnitBusy", "MemUnitStalled", "WriteUnitStalled"}});
        }
        else
        {
            groups.push_back(devtrace::DerivedSpmGroup{"Memory (%)", "", {"MemUnitBusy", "MemUnitStalled"}});
        }

        // Memory (bytes) counters
        counters.push_back(devtrace::DerivedSpmCounter{"FetchSize", "", "Fetch size", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"WriteSize", "", "Write size", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"LocalVidMemBytes", "", "Local video memory bytes", {}});
        counters.push_back(devtrace::DerivedSpmCounter{"PcieBytes", "", "PCIe bytes", {}});
        groups.push_back(devtrace::DerivedSpmGroup{"Memory (bytes)", "", {"FetchSize", "WriteSize", "LocalVidMemBytes", "PcieBytes"}});

        return Result::kSuccess;
    }

    tl::expected<void, std::string> GpaSpmCounterHandler::OpenGpaCounterContext(const system_info_utils::GpuInfo& target_gpu)
    {
        static const GpaUInt32 kAmdVendorId = 0x1002;

        auto* entry_points = GPUPerfAPICountersEntryPoints::Instance();
        if (nullptr == gpa_counter_context_)
        {
            if (!entry_points->EntryPointsValid())
            {
                return tl::make_unexpected(std::string("GPA counters entry points not valid."));
            }

            // NOTE: we are always using kGpaApiVulkan here. Since all officially-supported APIs are PAL-based, we need to
            // specify one of the PAL-based APIs. If we used the actual API, things would not be handled correctly for OpenCL,
            // which uses fewer counter block instances for per-SE blocks (it auto-sums them). Since the counter data is populated
            // by PAL, using Vulkan here makes sure that Vulkan, DX12, OpenCL and HIP are all handled correctly within GPA.
            // Also: we use Vulkan instead of DX12 because GPA doesn't support DX12 on Linux.
            // We may need to revisit this for non-PAL-based (i.e. GENERIC) APIs
            GpaCounterContextHardwareInfo counter_context_hardware_info = {
                kAmdVendorId, target_gpu.asic.id_info.device, target_gpu.asic.id_info.revision, nullptr, 0};

            GpaStatus gpa_status =
                entry_points->GetFuncTable()->GpaCounterLibOpenCounterContext(kGpaApiVulkan,
                                                                              kGpaSessionSampleTypeDiscreteCounter,
                                                                              counter_context_hardware_info,
                                                                              kGpaOpenContextDefaultBit | kGpaOpenContextEnableHardwareCountersBit,
                                                                              &gpa_counter_context_);
            if (kGpaStatusOk != gpa_status)
            {
                std::stringstream ss;
                if (kGpaStatusErrorHardwareNotSupported == gpa_status)
                {
                    ss << "Hardware not supported by GPUPerfAPI (asic_device_id= " << target_gpu.asic.id_info.device
                       << ", asic_revision=" << target_gpu.asic.id_info.revision << ", asic_family= " << target_gpu.asic.id_info.family
                       << "). Counter collection will be disabled.";
                }
                else
                {
                    ss << "Unable to open counter context (asic_device_id= " << target_gpu.asic.id_info.device
                       << ", asic_revision=" << target_gpu.asic.id_info.revision << ", gpa_status = " << gpa_status
                       << "). Counter collection will be disabled.";
                }
                return tl::make_unexpected(ss.str());
            }
        }

        return {};
    }

    bool GpaSpmCounterHandler::CloseGpaCounterContext()
    {
        auto* entry_points = GPUPerfAPICountersEntryPoints::Instance();
        if ((nullptr != gpa_counter_context_) && (entry_points->EntryPointsValid()))
        {
            GpaStatus gpa_status = entry_points->GetFuncTable()->GpaCounterLibCloseCounterContext(gpa_counter_context_);
            if (kGpaStatusOk == gpa_status)
            {
                gpa_counter_context_ = nullptr;
                return true;
            }
        }
        return false;
    }
}  // namespace devtrace
