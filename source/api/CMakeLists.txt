project(RdpCaptureApi)

# Parse version from the single source of truth: RdpCaptureApi.h
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/RdpCaptureApi.h" _ver_major REGEX "#define RDP_CAPTURE_API_VERSION_MAJOR [0-9]+")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/RdpCaptureApi.h" _ver_minor REGEX "#define RDP_CAPTURE_API_VERSION_MINOR [0-9]+")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/RdpCaptureApi.h" _ver_patch REGEX "#define RDP_CAPTURE_API_VERSION_PATCH [0-9]+")
string(REGEX MATCH "[0-9]+$" RDP_CAPTURE_API_MAJOR_VERSION "${_ver_major}")
string(REGEX MATCH "[0-9]+$" RDP_CAPTURE_API_MINOR_VERSION "${_ver_minor}")
string(REGEX MATCH "[0-9]+$" RDP_CAPTURE_API_PATCH_VERSION "${_ver_patch}")
set(RDP_CAPTURE_API_VERSION "${RDP_CAPTURE_API_MAJOR_VERSION}.${RDP_CAPTURE_API_MINOR_VERSION}.${RDP_CAPTURE_API_PATCH_VERSION}")

include(GenerateExportHeader)

set(SOURCES
        RdpCaptureApi.h
        api_allocator.cpp
        api_allocator.h
        api_blocklist.cpp
        api_blocklist.h
        api_clocks.cpp
        api_clocks.h
        api_data.h
        api_logger.cpp
        api_logger.h
        api_summary_generator.h
        api_table.cpp
        api_tool.cpp
        api_tool.h
        api_trace_io.cpp
        api_trace_io.h
        capture_context.cpp
        capture_context.h
        capture_params.cpp
        capture_params.h
        client_manager.cpp
        client_manager.h
        trace_source_adapter.cpp
        trace_source_adapter.h
        trace_source_provider.h)

set(WRAPPER_SOURCES
        "wrapper/capture_api_wrapper.cpp"
        "wrapper/capture_api_wrapper.h"
        "wrapper/timer.h")

add_library(${PROJECT_NAME} SHARED ${SOURCES})
add_library(RdpCaptureApi::RdpCaptureApi ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${RDP_CAPTURE_API_VERSION}
        SOVERSION ${RDP_CAPTURE_API_MAJOR_VERSION}
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
devtools_target_options(${PROJECT_NAME})

# Generate the import/export visibility header for all platforms. CMake defines
# RdpCaptureApi_EXPORTS only when compiling this shared library's own sources, so
# RDP_CAPTURE_API_EXPORT expands to dllexport/visibility("default") in the library
# and dllimport/empty in consumers automatically.
generate_export_header(${PROJECT_NAME}
        BASE_NAME RDP_CAPTURE_API
        EXPORT_MACRO_NAME RDP_CAPTURE_API_EXPORT
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/rdp_capture_api_export.h)

if (WIN32)
        target_sources(${PROJECT_NAME}
                PRIVATE
                win/RdpCaptureApi.rc)
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE
        ${LIBDEVTRACE_DIR})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${JSON_LIB_DIR})

# Public include for in-tree (add_subdirectory) and installed (find_package) consumers.
# The binary dir is where generate_export_header writes rdp_capture_api_export.h.
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:inc>)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        LibDevTrace
        gpa_counters_headers
        SystemTraceModuleStatic
        ddSiphonModule
        ddRouterUtils
        dd_tools
        ddRouter
        amdrdf)

# GPA - Use the IMPORTED target from spm_db
if (WIN32)
    install(FILES $<TARGET_FILE:gpa_counters> DESTINATION bin/ COMPONENT api)
elseif (LINUX)
    install(FILES $<TARGET_FILE:gpa_counters> DESTINATION lib/ COMPONENT api)
endif ()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gpa_counters> $<TARGET_FILE_DIR:${PROJECT_NAME}>)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../frontend/blocklist.ini
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying blocklist.ini to API output directory")

install(TARGETS ${PROJECT_NAME}
        EXPORT RdpCaptureApiTargets
        RUNTIME DESTINATION bin/ COMPONENT api
        ARCHIVE DESTINATION lib/ COMPONENT api
        LIBRARY DESTINATION lib/ COMPONENT api)
install(FILES
        RdpCaptureApi.h
        ${CMAKE_CURRENT_BINARY_DIR}/rdp_capture_api_export.h
        DESTINATION inc/ COMPONENT api)

# CMake package config for find_package(RdpCaptureApi)
include(CMakePackageConfigHelpers)

configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/RdpCaptureApiConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/RdpCaptureApiConfig.cmake"
        INSTALL_DESTINATION lib/cmake/RdpCaptureApi)

write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/RdpCaptureApiConfigVersion.cmake"
        VERSION "${RDP_CAPTURE_API_VERSION}"
        COMPATIBILITY SameMajorVersion)

install(EXPORT RdpCaptureApiTargets
        FILE RdpCaptureApiTargets.cmake
        NAMESPACE RdpCaptureApi::
        DESTINATION lib/cmake/RdpCaptureApi
        COMPONENT api)

install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/RdpCaptureApiConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/RdpCaptureApiConfigVersion.cmake"
        DESTINATION lib/cmake/RdpCaptureApi
        COMPONENT api)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION . COMPONENT self_capture_apps)

install(FILES ${WRAPPER_SOURCES} DESTINATION wrapper/ COMPONENT api)

# Ship the standalone helloworld sample (CMake project, vcpkg manifest, shaders,
# and sources) so consumers have a buildable example against the installed package.
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/examples/helloworld/
        DESTINATION examples/helloworld
        COMPONENT api)

if (WIN32)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION bin/ COMPONENT api)
endif ()

# Temporary project to do some testing
add_executable(capture_api_example)
devtools_target_options(capture_api_example)
target_sources(capture_api_example PRIVATE examples/example.cpp)
target_include_directories(capture_api_example PRIVATE
        ${LIBDEVTRACE_DIR})
target_include_directories(capture_api_example SYSTEM PRIVATE
        ${JSON_LIB_DIR})

target_link_libraries(capture_api_example ${PROJECT_NAME} LibDevTrace
        gpa_counters_headers
        SystemTraceModuleStatic
        ddSiphonModule
        ddRouterUtils
        dd_tools
        ddRouter
        amdrdf)

# Pure C99 consumer of the public RdpCaptureApi.h header. Acts as a compile gate
# ensuring the public C API remains usable from standard C callers. Links only the
# shared library since the example includes only the public header.
add_executable(capture_api_example_c examples/example.c)
target_link_libraries(capture_api_example_c PRIVATE ${PROJECT_NAME})
set_target_properties(capture_api_example_c PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF)

# Doxygen documentation for the public RdpCaptureApi C API.
# Gated behind the repo-wide RDP_ENABLE_DOXYGEN option; build with target RdpCaptureApiDocs.
if (RDP_ENABLE_DOXYGEN)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_PROJECT_NAME       "RDP Capture API")
        set(DOXYGEN_PROJECT_NUMBER     "${RDP_CAPTURE_API_VERSION}")
        set(DOXYGEN_PROJECT_BRIEF      "Developer Mode capture API for writing RGP profiles and RMV traces.")
        set(DOXYGEN_OUTPUT_DIRECTORY   "${CMAKE_CURRENT_BINARY_DIR}/doxygen")

        # Public C API: document the declared surface, not internal implementation details.
        set(DOXYGEN_EXTRACT_ALL        YES)
        set(DOXYGEN_EXTRACT_PRIVATE    NO)
        set(DOXYGEN_EXTRACT_STATIC     NO)
        set(DOXYGEN_HIDE_UNDOC_MEMBERS YES)

        # Tree-view sidebar for navigation.
        set(DOXYGEN_GENERATE_TREEVIEW  YES)
        set(DOXYGEN_DISABLE_INDEX      NO)
        set(DOXYGEN_FULL_SIDEBAR       NO)

        # Show file names without the absolute path prefix.
        set(DOXYGEN_FULL_PATH_NAMES    YES)
        set(DOXYGEN_STRIP_FROM_PATH    "${CMAKE_CURRENT_SOURCE_DIR}")

        set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)

        # Use the user guide as the documentation landing page.
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/USER_GUIDE.md")

        doxygen_add_docs(RdpCaptureApiDocs
                "${CMAKE_CURRENT_SOURCE_DIR}/USER_GUIDE.md"
                "${CMAKE_CURRENT_SOURCE_DIR}/RdpCaptureApi.h"
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/example.cpp"
                COMMENT "Generating RdpCaptureApi documentation with Doxygen")
    else ()
        message(STATUS "Doxygen not found, skipping RdpCaptureApi documentation generation.")
    endif ()
endif ()

