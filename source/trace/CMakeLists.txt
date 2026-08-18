#######################################################################################################################
### Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

add_library(LibDevTrace STATIC)
devtools_target_options(LibDevTrace)
target_link_libraries(LibDevTrace
        PUBLIC
        MercuryModuleExt
        ddCommon
        ddModuleUtil
        ddModule
        ddRdf
        dd_settings
        system_info
        spm_db
        gpa_counters_headers
        PRIVATE
        dd_tools
        ddRdf
        MercuryModuleExt
        PRIVATE
        ddCommon
        ddModuleUtil
)

set_target_properties(LibDevTrace
        PROPERTIES
        FOLDER "LibDevTrace"
)

target_include_directories(LibDevTrace
        PUBLIC
        ../third_party/expected/include
        PRIVATE
        inc
        ${EXTERNAL_DIR}
        # Includes the system info json parser which (hopefully) will eventually be moved
        ../modules/common/inc
)

target_include_directories(LibDevTrace
        PUBLIC
        ${FMT_LIB_DIR}/include
)
target_include_directories(LibDevTrace SYSTEM PUBLIC
        ${JSON_LIB_DIR}
)

target_sources(LibDevTrace PRIVATE
        src/json/json_mapper.cpp
        src/asic_info.cpp
        src/cancellable_timer.cpp
        src/chunk_writing.cpp
        src/dev_trace_common.cpp
        src/device_clocks.cpp
        inc/device_clocks.h
        src/dipper.cpp
        src/source_status.cpp
        src/source_userdata.cpp
        src/source_userdata_mapper.cpp
        src/trace_source_factory.cpp
        src/trace_io.cpp
        src/logging.cpp
        src/overlay_manager.cpp
        src/system_info_cache.cpp
        src/client_connection_manager.cpp
        src/standard_timer.cpp

        src/rmv/rmv_trace_source_impl.h
        src/rmv/rmv_trace_source_impl.cpp

        src/rgd/rgd_byte_writer.h
        src/rgd/rgd_byte_writer.cpp
        src/rgd/rgd_chunk_writer.h
        src/rgd/rgd_chunk_writer.cpp
        src/rgd/rgd_client.h
        src/rgd/rgd_client.cpp
        src/rgd/rgd_trace_source_impl.h
        src/rgd/rgd_trace_source_impl.cpp
        src/rgd/rgd_ubertrace_client.h
        src/rgd/rgd_ubertrace_client.cpp

        src/counters/rgp_gpa_counters_loader.cpp
        src/counters/rgp_spm_counter_handler.cpp
        src/counters/rgp_derived_spm_database.cpp

        src/rgp/active_gpu_provider.cpp
        src/rgp/active_gpu_provider.h
        src/rgp/rgp_pal_client_info.cpp
        src/rgp/rgp_trace_source_impl.h
        src/rgp/rgp_trace_source_impl.cpp
        src/rgp/rgp_legacy_trace_client.h
        src/rgp/rgp_legacy_trace_client.cpp
        src/rgp/rgp_ubertrace_client.h
        src/rgp/rgp_ubertrace_client.cpp

        src/rra/rra_trace_source_impl.h
        src/rra/rra_trace_source_impl.cpp

        src/base_trace_source/file_writing.cpp
        src/base_trace_source/ubertrace_client.cpp
        src/base_trace_source/ubertrace_features.cpp
        src/base_trace_source/ubertrace_params.cpp
        src/ubertrace_factory.cpp

        src/connection/supported_connection_set.cpp

        "inc/logging_definitions.h")

if (WIN32)
    target_compile_options(LibDevTrace PRIVATE /bigobj /wd4005)
endif ()
