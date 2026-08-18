// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

/// @author AMD Developer Tools Team
/// @file
/// @brief  Implementation for the UberTrace parameter JSON mapper.

#include "ubertrace_params.h"

#include "dev_trace_common.h"
#include "ubertrace_features.h"

namespace devtrace
{
    UbertraceControllerConfig::UbertraceControllerConfig(bool enabled)
        : enabled_(enabled)
    {
    }

    UbertraceControllerConfig::~UbertraceControllerConfig() = default;

    bool UbertraceControllerConfig::Map(JsonMapper mapper)
    {
        return mapper["enabled"](enabled_);
    }

    UbertraceFrameControllerConfig::UbertraceFrameControllerConfig(bool               enabled,
                                                                   uint32_t           num_prep_frames,
                                                                   uint32_t           num_capture_frames,
                                                                   const std::string& capture_mode,
                                                                   uint32_t           prep_start_index)
        : UbertraceControllerConfig(enabled)
        , num_prep_frames_(num_prep_frames)
        , num_capture_frames_(num_capture_frames)
        , capture_mode_(capture_mode)
        , prep_start_index_(prep_start_index)
    {
    }

    bool UbertraceFrameControllerConfig::Map(JsonMapper mapper)
    {
        return UbertraceControllerConfig::Map(mapper) && mapper["numPrepFrames"](num_prep_frames_) && mapper["captureFrameCount"](num_capture_frames_) &&
               mapper["captureMode"](capture_mode_) && mapper["preparationStartIndex"](prep_start_index_);
    }

    UbertraceRenderOpControllerConfig::UbertraceRenderOpControllerConfig(bool               enabled,
                                                                         const std::string& render_op,
                                                                         uint32_t           num_prep_ops,
                                                                         uint32_t           num_capture_ops,
                                                                         const std::string& capture_mode,
                                                                         uint32_t           prep_start_index)
        : UbertraceControllerConfig(enabled)
        , render_op_(render_op)
        , num_prep_ops_(num_prep_ops)
        , num_capture_ops_(num_capture_ops)
        , capture_mode_(capture_mode)
        , prep_start_index_(prep_start_index)
    {
    }

    bool UbertraceRenderOpControllerConfig::Map(JsonMapper mapper)
    {
        return UbertraceControllerConfig::Map(mapper) && mapper["renderOpMode"](render_op_) && mapper["numPrepRenderOps"](num_prep_ops_) &&
               mapper["captureRenderOpCount"](num_capture_ops_) && mapper["captureMode"](capture_mode_) &&
               mapper["preparationStartRenderOp"](prep_start_index_);
    }

    UbertraceMarkerControllerConfig::UbertraceMarkerControllerConfig(const bool         enabled,
                                                                     const std::string& start_marker_string,
                                                                     const std::string& end_marker_string)
        : UbertraceControllerConfig(enabled)
        , start_marker_string_(start_marker_string)
        , end_marker_string_(end_marker_string)
    {
    }

    bool UbertraceMarkerControllerConfig::Map(JsonMapper mapper)
    {
        return UbertraceControllerConfig::Map(mapper) && mapper["startMarkerString"](start_marker_string_) && mapper["endMarkerString"](end_marker_string_);
    }

    static bool MapController(UbertraceController& controller, JsonMapper mapper)
    {
        return mapper["name"](controller.name) && (controller.config == nullptr || controller.config->Map(mapper["config"]));
    }

    UbertraceControllerCollection::UbertraceControllerCollection(const UbertraceFeatures& features, const UbertraceController& controller)
        : use_new_format_(features.UseNewControllerFormat())
        , controller_(controller)
    {
    }

    bool UbertraceControllerCollection::Map(JsonMapper& mapper)
    {
        DEV_TRACE_ASSERT(mapper.IsSerializing());

        if (use_new_format_)
        {
            return MapController(controller_, mapper["controller"]);
        }

        std::vector<UbertraceController> controllers = {controller_};
        return mapper["controllers"].Arr<UbertraceController>(controllers, &MapController);
    }

    static bool MapSource(UberTraceSource& source, JsonMapper mapper)
    {
        return mapper["name"](source.name) && (source.config == nullptr || source.config->Map(mapper));
    }

    static bool Map(UberTraceConfig& config, JsonMapper& mapper)
    {
        bool result = mapper["sources"].Arr<UberTraceSource>(config.sources, &MapSource) && (config.controllers == nullptr || config.controllers->Map(mapper));

        // Map global params under "global" key if present
        if (result && config.global_params != nullptr)
        {
            result = config.global_params->Map(mapper["global"]);
        }

        return result;
    }

    tl::expected<std::string, std::string> UberTraceConfigSerializer::Serialize(UberTraceConfig& config)
    {
        JsonMapper mapper = JsonMapper::Serialize();

        if (!Map(config, mapper))
        {
            return tl::make_unexpected("Failed to map UberTrace config for serialization");
        }

        return mapper.ToString();
    }
};  // namespace devtrace
