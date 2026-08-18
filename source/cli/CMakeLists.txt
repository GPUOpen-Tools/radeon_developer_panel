#######################################################################################################################
### Copyright (c) 2024-2026 Advanced Micro Devices, Inc. All rights reserved.
### @author AMD Developer Tools Team
#######################################################################################################################

project(RadeonDeveloperPanelCLI)

# cxxopts for command line parsing
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cxxopts ${CMAKE_CURRENT_BINARY_DIR}/cxxopts)

# Generate the built-in blocklist header from the shared blocklist.ini so the
# CLI's hardcoded defaults stay in sync with the GUI's defaults at build time.
find_package(Python COMPONENTS Interpreter REQUIRED)
set(CLI_BLOCKLIST_INI ${CMAKE_CURRENT_SOURCE_DIR}/../frontend/blocklist.ini)
set(CLI_BLOCKLIST_GEN ${CMAKE_CURRENT_SOURCE_DIR}/blocklist_gen.py)
set(CLI_BLOCKLIST_HEADER ${CMAKE_CURRENT_BINARY_DIR}/cli_blocklist_defaults.h)
add_custom_command(
    OUTPUT ${CLI_BLOCKLIST_HEADER}
    DEPENDS ${CLI_BLOCKLIST_INI} ${CLI_BLOCKLIST_GEN}
    COMMAND ${Python_EXECUTABLE} ${CLI_BLOCKLIST_GEN} ${CLI_BLOCKLIST_INI} ${CLI_BLOCKLIST_HEADER}
    COMMENT "Generating CLI blocklist defaults header"
)
# Wrap the OUTPUT rule in an explicit target so multi-config IDE generators
# (Visual Studio, Xcode) reliably run the generation step before compilation.
# Mirrors the pattern in source/trace/CMakeLists.txt.
add_custom_target(cli_blocklist_defaults DEPENDS ${CLI_BLOCKLIST_HEADER})

set(SOURCES
    "main.cpp"
    "capture_cli.cpp"
    "capture_cli.h"
    ${CLI_BLOCKLIST_HEADER}
)

if (WIN32)
    set(WINDOWS_SOURCES
        "../version.h"
        "win/resource.h"
        "win/cli.rc"
    )
endif()

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})
if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE ${WINDOWS_SOURCES})
endif()

devtools_target_options(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../api
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    ${JSON_LIB_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/expected/include
)

target_link_libraries(${PROJECT_NAME}
    cxxopts::cxxopts
    RdpCaptureApi
    amdrdf
)

# Ensure CLI depends on the API target and on the generated blocklist header
add_dependencies(${PROJECT_NAME} RdpCaptureApi cli_blocklist_defaults)

# Copy RdpCaptureApi DLL/SO to the CLI output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:RdpCaptureApi>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying RdpCaptureApi to CLI output directory"
)

# Copy GPA library (required by RdpCaptureApi), using the IMPORTED target from spm_db
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:gpa_counters>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying GPA library to CLI output directory"
)

if (LINUX)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
endif()

# Install the CLI executable
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION . COMPONENT apps
    BUNDLE DESTINATION . COMPONENT apps
    ARCHIVE DESTINATION lib/ COMPONENT apps
    LIBRARY DESTINATION lib/ COMPONENT apps)

# Install RdpCaptureApi alongside the CLI
install(TARGETS RdpCaptureApi
    RUNTIME DESTINATION . COMPONENT apps
    LIBRARY DESTINATION lib/ COMPONENT apps)

# Install PDB files for Windows non-release builds
if (WIN32)
    if (NOT CMAKE_BUILD_TYPE MATCHES "Release")
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION . OPTIONAL COMPONENT apps)
    endif ()
endif ()
