// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Declaration for the UberTrace parameter JSON serializer.

#ifndef RDP_SOURCE_TRACE_SRC_UBERTRACE_UBERTRACE_PARAMS_H_
#define RDP_SOURCE_TRACE_SRC_UBERTRACE_UBERTRACE_PARAMS_H_

#include <memory>
#include <string>
#include <vector>

#include <dd_gpu_profiling_api.h>

#include "json/json_mapper.h"
#include "tl/expected.hpp"

namespace devtrace
{
    /// @brief Configuration for an UberTrace controller.
    class UbertraceControllerConfig
    {
    public:
        /// @brief Constructor.
        /// @param [in] enabled true if the controller is enabled, false otherwise.
        explicit UbertraceControllerConfig(bool enabled);

        virtual ~UbertraceControllerConfig();

        /// @brief Maps this config to JSON.
        /// @param [in] mapper The object to use to map to JSON.
        /// @return true if mapping was successful, false otherwise.
        virtual bool Map(JsonMapper mapper);

    private:
        bool enabled_ = false;  ///< true if the controller is enabled, false otherwise.
    };

    /// @brief Config for the UberTrace frame controller.
    class UbertraceFrameControllerConfig final : public UbertraceControllerConfig
    {
    public:
        /// @brief Constructor.
        /// @param [in] enabled true if the controller is enabled, false otherwise.
        /// @param [in] num_prep_frames The number of prep frames that the capture should have.
        /// @param [in] num_capture_frames The number of frames to capture.
        /// @param [in] capture_mode The capture mode.
        /// @param [in] prep_start_index The index of the start of preparation.
        UbertraceFrameControllerConfig(bool               enabled,
                                       uint32_t           num_prep_frames,
                                       uint32_t           num_capture_frames,
                                       const std::string& capture_mode,
                                       uint32_t           prep_start_index);

        bool Map(JsonMapper mapper) override;

    private:
        uint32_t num_prep_frames_    = 0;  ///< The number of prep frames to use.
        uint32_t num_capture_frames_ = 1;  ///< The number of frames to capture.

        std::string capture_mode_     = "relative";  ///< The capture mode.
        uint32_t    prep_start_index_ = 0;           ///< The index of the start of preparation.
    };

    /// @brief Config for the UberTrace render op controller.
    class UbertraceRenderOpControllerConfig final : public UbertraceControllerConfig
    {
    public:
        /// @brief Constructor.
        /// @param [in] enabled true if the controller is enabled, false otherwise.
        /// @param [in] render_op The render op to capture.
        /// @param [in] num_prep_ops The number of prep render ops that the capture should have.
        /// @param [in] num_capture_ops The number of render ops to capture.
        /// @param [in] capture_mode The capture mode.
        /// @param [in] prep_start_index The index of the start of preparation.
        UbertraceRenderOpControllerConfig(bool               enabled,
                                          const std::string& render_op,
                                          uint32_t           num_prep_ops,
                                          uint32_t           num_capture_ops,
                                          const std::string& capture_mode,
                                          uint32_t           prep_start_index);

        bool Map(JsonMapper mapper) override;

    private:
        std::string render_op_       = "draw";  ///< The render op to capture.
        uint32_t    num_prep_ops_    = 0;       ///< The number of prep render ops that the capture should have.
        uint32_t    num_capture_ops_ = 1;       ///< The number of render ops to capture.

        std::string capture_mode_     = "relative";  ///< The capture mode.
        uint32_t    prep_start_index_ = 0;           ///< The index of the start of preparation.
    };

    /// @brief Config for the UberTrace marker controller.
    class UbertraceMarkerControllerConfig final : public UbertraceControllerConfig
    {
    public:
        /// @brief Constructor.
        /// @param [in] enabled true if the controller is enabled, false otherwise.
        /// @param [in] start_marker_string The marker string that starts the capture.
        /// @param [in] end_marker_string The marker string that ends the capture.
        UbertraceMarkerControllerConfig(bool enabled, const std::string& start_marker_string, const std::string& end_marker_string);

        bool Map(JsonMapper mapper) override;

    private:
        std::string start_marker_string_;  ///< The marker string that starts the capture.
        std::string end_marker_string_;    ///< The marker string that ends the capture.
    };

    /// @brief Definition for an UberTrace controller.
    struct UbertraceController
    {
        std::string                                name = "";  ///< The name of the controller.
        std::shared_ptr<UbertraceControllerConfig> config;     ///< The configuration for the controller.
    };

    class UbertraceControllerCollection
    {
    public:
        /// @brief Constructor.
        /// @param [in] features  The UberTrace driver features.
        /// @param [in] controller The controller to use.
        explicit UbertraceControllerCollection(const struct UbertraceFeatures& features, const UbertraceController& controller);

        /// @brief Maps this config to JSON.
        /// @param [in] mapper The object to use to map to JSON.
        /// @return true if mapping was successful, false otherwise.
        bool Map(JsonMapper& mapper);

    private:
        bool                use_new_format_ = false;  ///< true if the new single controller should be used.
        UbertraceController controller_;              ///< The controller to use.
    };

    /// @brief Configuration for an UberTrace data source.
    class UbertraceSourceConfig
    {
    public:
        /// @brief Constructor.
        UbertraceSourceConfig() = default;

        /// @brief Destructor.
        virtual ~UbertraceSourceConfig() = default;

        /// @brief Maps this config to JSON.
        /// @param [in] mapper The object to use to map to JSON.
        /// @return true if mapping was successful, false otherwise.
        virtual bool Map([[maybe_unused]] JsonMapper& mapper)
        {
            return true;
        }
    };

    /// @brief Configuration for an UberTrace data source that has an enabled flag.
    class EnableDisableUberTraceSourceConfig : public UbertraceSourceConfig
    {
    public:
        /// @brief Constructor.
        /// @param enabled true if the trace source is enabled, false otherwise.
        explicit EnableDisableUberTraceSourceConfig(bool enabled)
        {
            enabled_ = enabled;
        }

        ~EnableDisableUberTraceSourceConfig() override = default;

        bool Map(JsonMapper& mapper) override
        {
            return mapper["config"]["enabled"](this->enabled_);
        }

    private:
        bool enabled_;  ///< Flag if source is enabled
    };

    /// @brief Ray history trace source config.
    class UberTraceRayHistorySourceConfig final : public EnableDisableUberTraceSourceConfig
    {
    public:
        /// @brief Constructor.
        /// @param [in] enabled true if ray history should be enabled, false otherwise.
        /// @param [in] ray_history_buffer_size History buffer size.
        UberTraceRayHistorySourceConfig(const bool enabled, const uint64_t ray_history_buffer_size)
            : EnableDisableUberTraceSourceConfig(enabled)
            , ray_history_buffer_size_(ray_history_buffer_size)
        {
        }

        bool Map(JsonMapper& mapper) override
        {
            return EnableDisableUberTraceSourceConfig::Map(mapper) && mapper["config"]["rayHistoryBufferSizeBytes"](ray_history_buffer_size_);
        }

    private:
        uint64_t ray_history_buffer_size_;  ///< History buffer size.
    };

    /// @brief UberTrace config for Gpu perf experiment
    class UberTraceGpuPerfSourceConfig : public UbertraceSourceConfig
    {
    public:
        /// Constructor.
        /// @param [in] enable_sqtt true if SQTT should be enabled, false otherwise.
        /// @param [in] sqtt_memory_limit The memory limit in mb for SQTT.
        /// @param [in] enable_inst_tokens true if instruction tracing should be enabled.
        /// @param [in] enable_exec_pop_tokens true if Exec/Pop tokens should be enabled, false otherwise.
        /// @param [in] se_mask The shader engine mask for SQTT.
        UberTraceGpuPerfSourceConfig(const bool     enable_sqtt,
                                     const uint64_t sqtt_memory_limit,
                                     const bool     enable_inst_tokens,
                                     const bool     enable_exec_pop_tokens,
                                     const uint32_t se_mask)
            : UbertraceSourceConfig()
            , enable_sqtt_(enable_sqtt)
            , sqtt_memory_limit_(sqtt_memory_limit)
            , enable_inst_tokens_(enable_inst_tokens)
            , enable_exec_pop_tokens_(enable_exec_pop_tokens)
            , se_mask_(se_mask)
        {
        }

        bool Map(JsonMapper& mapper) override
        {
            return UbertraceSourceConfig::Map(mapper) && mapper["config"]["sqtt"]["enabled"](enable_sqtt_) && mapper["config"]["spm"]["enabled"](enable_spm_) &&
                   mapper["config"]["sqtt"]["memoryLimitInMb"](sqtt_memory_limit_) &&
                   mapper["config"]["sqtt"]["enableInstructionTokens"](enable_inst_tokens_) &&
                   mapper["config"]["sqtt"]["enableExecPopTokens"](enable_exec_pop_tokens_) && mapper["config"]["sqtt"]["seMask"](se_mask_);
        }

    private:
        bool     enable_spm_ = false;      ///< Always false for this config.
        bool     enable_sqtt_;             ///< true if SQTT should be enabled, false otherwise.
        uint64_t sqtt_memory_limit_;       ///< The memory limit for SQTT.
        bool     enable_inst_tokens_;      ///< true if instruction tokens should be enabled, false otherwise.
        bool     enable_exec_pop_tokens_;  ///< true if Exec/Pop count tokens should be enabled.
        uint32_t se_mask_;                 ///< The shader engine mask for SQTT.
    };

    /// @brief UberTrace config for Gpu perf experiment
    class UberTraceGpuPerfSpmSourceConfig final : public UberTraceGpuPerfSourceConfig
    {
    public:
        /// Constructor.
        /// @param [in] enable_sqtt true if SQTT should be enabled, false otherwise.
        /// @param [in] sqtt_memory_limit The memory limit in mb for SQTT.
        /// @param [in] enable_inst_tokens true if instruction tracing should be enabled.
        /// @param [in] enable_exec_pop_tokens true if Exec/Pop tokens should be enabled, false otherwise.
        /// @param [in] se_mask The shader engine mask for instruction tracing.
        /// @param [in] counters SPM Counters to request.
        /// @param [in] sample_frequency The frequency to sample SPM at.
        /// @param [in] spm_memory_limit The memory limit for SPM in MB.
        UberTraceGpuPerfSpmSourceConfig(const bool                                     enable_sqtt,
                                        const uint64_t                                 sqtt_memory_limit,
                                        const bool                                     enable_inst_tokens,
                                        const bool                                     enable_exec_pop_tokens,
                                        const uint32_t                                 se_mask,
                                        const std::vector<DDGpuProfilingSpmCounterId>& counters,
                                        const uint32_t                                 sample_frequency,
                                        const uint32_t                                 spm_memory_limit)
            : UberTraceGpuPerfSourceConfig(enable_sqtt, sqtt_memory_limit, enable_inst_tokens, enable_exec_pop_tokens, se_mask)
            , counters_(counters)
            , sample_frequency_(sample_frequency)
            , spm_memory_limit_(spm_memory_limit)
        {
        }

        bool Map(JsonMapper& mapper) override
        {
            bool spm_enabled = true;
            return UberTraceGpuPerfSourceConfig::Map(mapper) && mapper["config"]["spm"]["enabled"](spm_enabled) &&
                   mapper["config"]["spm"]["perfCounters"].Arr<DDGpuProfilingSpmCounterId>(counters_, MapCounter, {}) &&
                   mapper["config"]["spm"]["sampleFrequency"](sample_frequency_) && mapper["config"]["spm"]["memoryLimitInMb"](spm_memory_limit_);
        }

    private:
        /// @brief Maps the counter to and from JSON.
        /// @param [in] counter The counter to map.
        /// @param [in] mapper The JSON mapper to use.
        /// @return true if mapping was successful, false otherwise.
        static bool MapCounter(const DDGpuProfilingSpmCounterId& counter, JsonMapper mapper)
        {
            // Only valid for serialization
            DEV_TRACE_ASSERT(mapper.IsSerializing());

            std::vector ids = {counter.blockId, counter.instanceId, counter.eventId};
            return mapper(ids);
        }

        std::vector<DDGpuProfilingSpmCounterId> counters_;          ///< SPM Counters to request.
        uint32_t                                sample_frequency_;  ///< The frequency to sample SPM at.
        uint32_t                                spm_memory_limit_;  ///< The memory limit for SPM in MB.
    };

    /// @brief Definition for an UberTrace data source.
    class UberTraceSource
    {
    public:
        std::string                            name = "";  ///< The name of the source.
        std::shared_ptr<UbertraceSourceConfig> config;     ///< The configuration for the source (sent before capture).
    };

    /// @brief Global trace parameters (driver-level settings, not source-specific).
    struct UberTraceGlobalParams
    {
        bool enable_single_token_sqtt_write_ = false;  ///< Enable single token SQTT writes globally.

        /// @brief Maps this config to JSON.
        /// @param [in] mapper The object to use to map to JSON.
        /// @return true if mapping was successful, false otherwise.
        bool Map(JsonMapper mapper)
        {
            return mapper["enableSingleTokenSqttWrite"](enable_single_token_sqtt_write_);
        }
    };

    /// @brief Configuration for UberTrace.
    struct UberTraceConfig
    {
        std::unique_ptr<UbertraceControllerCollection> controllers{};    ///< The controllers to use for UberTrace.
        std::vector<UberTraceSource>                   sources{};        ///< The sources to use for UberTrace.
        std::unique_ptr<UberTraceGlobalParams>         global_params{};  ///< Global driver parameters (optional).
    };

    /// @brief An object that can serialize an UberTraceConfig to JSON.
    class UberTraceConfigSerializer
    {
    public:
        /// @brief Serializes the config to a UTF-8 JSON string.
        /// @param [in] config The configuration to serialize.
        /// @return The serialized JSON string or an error message.
        static tl::expected<std::string, std::string> Serialize(UberTraceConfig& config);

    private:
        /// @brief Constructor.
        UberTraceConfigSerializer() = default;
    };

}  // namespace devtrace

#endif
