#######################################################################################################################
### Copyright (c) 2019-2026 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0087 NEW)
cmake_policy(SET CMP0169 OLD)

project(RDP)

option(RDP_ENABLE_UNIT_TESTS "Enable unit tests" OFF)
option(RDP_ENABLE_DOXYGEN "Enable doxygen generation" OFF)
option(RDP_ENABLE_FEATURE_DRIVER_SETTINGS "Enable driver settings feature" OFF)
option(RDP_ALLOW_UNHANDLED_EXCEPTIONS "Enable unhandled exceptions" OFF)

if (RDP_ALLOW_UNHANDLED_EXCEPTIONS)
    add_compile_definitions(RDP_ALLOW_UNHANDLED_EXCEPTIONS)
endif()

# Enable PIE for libraries
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Define version information
set(RDP_MAJOR_VERSION 3)
set(RDP_MINOR_VERSION 5)
if (NOT RDP_PATCH_NUMBER)
    set(RDP_PATCH_NUMBER 0)
endif ()
if (NOT RDP_BUILD_NUMBER)
    set(RDP_BUILD_NUMBER 0)
endif ()
string(TIMESTAMP DATE "\"%m/%d/%Y\"")
string(TIMESTAMP YEAR "\"%Y\"")
if (NOT RDP_PACKAGE_FILE_NAME_SUFFIX)
    set(RDP_PACKAGE_FILE_NAME_SUFFIX "")
endif ()

if (WIN32)
    add_compile_options(/Wv:18)
endif (WIN32)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/source/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/source/version.h")

# Add cmake utilities
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
if (NOT HEADLESS)
    include(devtools_qt_helper)
endif (NOT HEADLESS)

# The sample apps are output in the same folder as the integration tests so they can use a
# relative path to reference them.
set(SAMPLE_APP_BINARY_DIR "${CMAKE_BINARY_DIR}/test/integration/sample_apps")

include(dev_tools)

include(fetch_external)

# Validate that SPM_DB created the required GPA IMPORTED targets
if(NOT TARGET gpa_counters)
    message(FATAL_ERROR "SPM_DB did not create gpa_counters target. GPA library may not have been found.")
endif()
if(NOT TARGET gpa_counters_headers)
    message(FATAL_ERROR "SPM_DB did not create gpa_counters_headers target.")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/source/deploy_qt_helper)

set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(MODULE_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/modules)

# RDP uses RapidJSON (header-only) from the devdriver third_party directory
set(JSON_LIB_DIR ${PROJECT_SOURCE_DIR}/external/devdriver/third_party/rapidjson/include)
set(FMT_LIB_DIR ${PROJECT_SOURCE_DIR}/source/third_party/fmt)

include_directories(${EXTERNAL_DIR})

if (WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)

    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . COMPONENT apps)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION scripts COMPONENT apps)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION . COMPONENT self_capture_apps)
endif (WIN32)

### LibDevTrace #######################################################################################################
add_subdirectory(source/trace)
set(LIBDEVTRACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/source/trace/inc)

### RdpCaptureAPI ######################################################################################################
add_subdirectory(source/api)

### RadeonDeveloperPanelCLI ############################################################################################
add_subdirectory(source/cli)

### DevTools Modules ##################################################################################################
add_subdirectory(source/modules)

### Radeon Developer Panel ###########################################################################################################
add_subdirectory(source/frontend)

### Radeon Developer Service ###########################################################################################################
add_subdirectory(source/service)

if (WIN32)
    add_subdirectory(source/registry)
endif ()

### Tests #############################################################################################################
if (RDP_ENABLE_UNIT_TESTS)
    enable_testing()
endif ()
add_subdirectory(test)

# For Windows release builds, generate .pdb files so they can be loaded for crash dumps
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_BUILD_TYPE MATCHES "Release")

    # Recursively gets all targets in a directory
    macro(get_all_targets targets dir)
        get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        foreach (subdir ${subdirectories})
            get_all_targets(${targets} ${subdir})
        endforeach ()

        get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        list(APPEND ${targets} ${current_targets})
    endmacro()

    # Generate .pdb files for all targets in the project so we get information for anywhere in the code
    get_all_targets(all_targets ${CMAKE_CURRENT_SOURCE_DIR})
    foreach (current_target IN LISTS all_targets)

        get_target_property(target_type ${current_target} TYPE)
        if (NOT ${target_type} STREQUAL "UTILITY" AND NOT ${target_type} STREQUAL "INTERFACE_LIBRARY")
            if (NOT ${target_type} STREQUAL "STATIC_LIBRARY")
                # /DEBUG changes the defaults for the /OPT option from REF to NOREF and from ICF to NOICF, so reset the defaults
                set_property(TARGET ${current_target} APPEND PROPERTY LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF")
            endif ()

            # Create .pdb files, don't embed the debug data in the executable
            target_compile_options(${current_target} PRIVATE /Zi)

        endif ()
    endforeach ()

    install(FILES $<TARGET_PDB_FILE:RadeonDeveloperPanel> DESTINATION . OPTIONAL COMPONENT debug)
endif ()

# Legal and Update notices
install(FILES NOTICES.txt DESTINATION . COMPONENT apps)
# Release notes
install(FILES RELEASE_NOTES.txt DESTINATION . COMPONENT apps)

# Help Documentation
find_program(SPHINX_EXECUTABLE sphinx-build)
if (SPHINX_EXECUTABLE)
    add_custom_target(Documentation ALL)
    if (NOT HEADLESS)
        add_dependencies(Documentation RadeonDeveloperPanel)
    endif ()
    add_custom_command(TARGET Documentation POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Building Sphinx documentation"
            COMMAND ${SPHINX_EXECUTABLE} ${SPHINX_OPTIONS} ${CMAKE_CURRENT_SOURCE_DIR}/documentation/source ${CMAKE_BINARY_DIR}/help/rdp/.)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${CMAKE_BINARY_DIR}/help)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/help DESTINATION . COMPONENT apps)
endif ()

# Scripts
if (LINUX)
    set(SCRIPT_FILES
            scripts/setup.sh
            scripts/remove_shared_memory.sh)
    install(FILES ${SCRIPT_FILES} DESTINATION scripts/ COMPONENT apps)
    if (CMAKE_SYSTEM_NAME MATCHES "Android")
        install(FILES scripts/rdp_android.py DESTINATION . COMPONENT apps)
    endif ()
elseif (WIN32)
    install(FILES scripts/AddUserToGroup.bat DESTINATION scripts/ COMPONENT apps)
    install(FILES scripts/AddUserToGroup.bat DESTINATION scripts/ COMPONENT self_capture_apps)
endif ()

# Set RadeonDeveloperPanel target as default for Visual Studio solutions
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT RadeonDeveloperPanel)

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING IGNORE)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    set(CPACK_ARCHIVE_APPS_FILE_NAME "RDP_Debug${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
    set(CPACK_ARCHIVE_API_FILE_NAME "RdpCaptureApi_Debug${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
    set(CPACK_ARCHIVE_SELF_CAPTURE_APPS_FILE_NAME "SelfCaptureApps_Debug${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
else ()
    set(CPACK_ARCHIVE_APPS_FILE_NAME "RDP${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
    set(CPACK_ARCHIVE_API_FILE_NAME "RdpCaptureApi${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
    set(CPACK_ARCHIVE_DEBUG_FILE_NAME "RDP_Debug_Symbols${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")
    set(CPACK_ARCHIVE_SELF_CAPTURE_APPS_FILE_NAME "SelfCaptureApps${RDP_PACKAGE_FILE_NAME_SUFFIX}_${RDP_MAJOR_VERSION}.${RDP_MINOR_VERSION}.${RDP_PATCH_NUMBER}.${RDP_BUILD_NUMBER}")

endif ()

include(CPack)

cpack_add_component(apps
        DISPLAY_NAME "Applications"
        DESCRIPTION "RDP, RDS, and RDS CLI applications")
cpack_add_component(debug
        DISPLAY_NAME "Debug Symbols"
        DESCRIPTION "Debug Symbols")
cpack_add_component(self_capture_apps
        DISPLAY_NAME "Self capture apps"
        DESCRIPTION "Self capture apps for RGP")

