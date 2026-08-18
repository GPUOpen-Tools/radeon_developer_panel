##############################################################################################################
### Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

# Create the unit tests executable
add_executable(
        RDPUnitTests

        # LibDevTrace
        trace/json/json_mapper_test.cpp
        trace/json/json_mapper_array_test.cpp

        # Common Module
        modules/common/formatting_test.cpp
        modules/common/input_validation_test.cpp

        # Profiling Module
        modules/profiling/spm_counter_file_parser_test.cpp

        #        trace/rra_trace_source_test.cpp
        #        trace/rgp_trace_source_test.cpp
        #        trace/rgp_trace_source_config_test.cpp
        #        trace/rmv_trace_source_test.cpp
        #        trace/rgd_trace_source_test.cpp
        #
        #        # TODO: Implement unit tests for system info
        #        #trace/system_info/system_info_parser_v3_test.cpp
        #        #trace/system_info/system_info_parser_v4_test.cpp
        #        #trace/system_info/system_info_cache_test.cpp
        #        trace/source_userdata_test.cpp

        # Trace mocks
        #trace/mock/mock_module_common.cpp
        #trace/mock/mock_module_common.h
        #trace/mock/mock_rmv_api.h
        #trace/mock/mock_rmv_api.cpp
        #trace/mock/mock_rgp_api.h
        #trace/mock/mock_rgp_api.cpp
        #trace/mock/mock_rra_api.h
        #trace/mock/mock_rra_api.cpp
        #trace/mock/mock_rgd_api.h
        #trace/mock/mock_rgd_api.cpp

        # Module mocks
        #modules/mock/mock_file_opener.h
        #modules/mock/mock_file_stream_provider.h
        #modules/mock/mock_file_utils.h
        #modules/mock/mock_settings.h
        #modules/mock/mock_settings.cpp
        #modules/mock/mock_trace_timer.h
        #modules/mock/mock_trace_timer.cpp
)

set_target_properties(RDPUnitTests PROPERTIES FOLDER "Test")
devtools_target_options(RDPUnitTests)

target_link_libraries(
        RDPUnitTests
        Qt::Core
        Qt::Test
        Qt::Widgets
        gmock
        gtest_main
        TestUtils

        # Link RDP libraries/ components
        DevToolsCommonStatic
        LibDevTrace
        #DevToolsMemoryTraceModule
        #DevToolsCrashAnalysisModule
        #DevToolsRaytracingModule
)

target_include_directories(RDPUnitTests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${EXTERNAL_DIR}
        ${EXTERNAL_DIR}/third_party
        ${LIBDEVTRACE_DIR}
        ${TEST_UTIL_DIR}
        ${MODULE_INC_DIR}

        # Includes the system info json parser
        ../../source/modules/common/inc
)
target_include_directories(RDPUnitTests SYSTEM PRIVATE
        ${JSON_LIB_DIR}
)

deploy_qt_build(RDPUnitTests FALSE)
gtest_discover_tests(RDPUnitTests DISCOVERY_TIMEOUT 60)
add_test(RDPUnitTests RDPUnitTests)

# Add any assets needed by the tests
add_custom_target(CopyUnitTestAssets COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
set_target_properties(CopyUnitTestAssets PROPERTIES FOLDER "Test")
add_dependencies(RDPUnitTests CopyUnitTestAssets)

add_compile_definitions(SYSTEM_INFO_JSON_FILENAME="assets/system_info.json")
add_compile_definitions(RMV_USERDATA_JSON_FILENAME="assets/rmv_userdata.json")
add_compile_definitions(RGP_USERDATA_JSON_FILENAME="assets/rgp_userdata.json")
add_compile_definitions(RRA_USERDATA_JSON_FILENAME="assets/rra_userdata.json")
add_compile_definitions(RGD_USERDATA_JSON_FILENAME="assets/rgd_userdata.json")

add_compile_definitions(SYSTEM_INFO_V3_JSON_FILENAME="assets/system_info_v3.json")
add_compile_definitions(SYSTEM_INFO_V4_JSON_FILENAME="assets/system_info_v4.json")
add_compile_definitions(RRA_UBER_TRACE_CONFIG_JSON_FILENAME="assets/rra_uber_trace_config.json")
