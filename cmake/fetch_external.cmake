# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

### \author AMD Developer Tools Team

if (RDP_COMPLETED_FETCH_DEPENDENCIES)
    return()
endif()

include(FetchContent)

# Each dependency is fetched from its public GPUOpen repository unless a local
# checkout is supplied via the matching <DEP>_DIR variable, in which case that
# directory is added directly. This lets developers point any dependency at a
# working copy without editing this file.

# system info
set(SYSTEM_INFO_BUILD_WRITER ON)
set(SYSTEM_INFO_BUILD_RDF_INTERFACES ON)
if (NOT SYSTEM_INFO_UTILS_DIR)
    FetchContent_Declare(
            system_info_utils
            GIT_REPOSITORY "https://github.com/GPUOpen-Tools/system_info_utils.git"
            GIT_TAG main
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/system_info_utils"
    )
    FetchContent_MakeAvailable(system_info_utils)
else ()
    add_subdirectory(${SYSTEM_INFO_UTILS_DIR} system_info_utils)
endif ()

# rdf
# Disable the C++ bindings for RDF as they are not used in RDP
# NOTE: This may have an affect on the RDTS testkit, so we should verify that as well.
# For context, the issue is that devdriver needs RDF but cannot used exceptions, which the C++ bindings use.
# So for RDP, we need to only use the C interface. But RRA, RMV, and other tools may need the C++ bindings.
set(RDF_ENABLE_CXX_BINDINGS OFF)
set(RDF_STATIC ON)
if (NOT LIBAMDRDF_DIR)
    FetchContent_Declare(
            rdf
            GIT_REPOSITORY "https://github.com/GPUOpen-Drivers/libamdrdf.git"
            GIT_TAG main
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/rdf"
    )
    FetchContent_MakeAvailable(rdf)
else ()
    add_subdirectory(${LIBAMDRDF_DIR} rdf)
endif ()

# Developer Mode Driver
set(DD_BRANCH_STRING "rc")
set(DD_BP_BUILD_MODULES ON)
set(DD_BUILD_RDF_MODULES ON)
set(DD_BP_ENABLE_TOOL_LIBRARIES ON)
set(DD_ENABLE_AMDLOG_CONNECTION ON)
set(DD_BP_MESSAGELIB_USE_PREBUILT_LIBRARY ON)
set(DD_BP_STANDARD_DRIVER_PROTOCOLS ON)
set(DD_BP_INSTALL OFF)
set(DD_BP_ENABLE_DD_MODULE_APIS ON)
set(DD_BP_ENABLE_DD_SETTINGS ON)
if (CMAKE_BUILD_TYPE MATCHES Release)
    set(DD_OPT_LOG_LEVEL LogLevel::Never)
endif ()
if (NOT DEVDRIVER_DIR)
    FetchContent_Declare(
            devdriver
            GIT_REPOSITORY "https://github.com/GPUOpen-Tools/devdriver.git"
            GIT_TAG main
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/devdriver"
    )
    FetchContent_MakeAvailable(devdriver)
else ()
    add_subdirectory(${DEVDRIVER_DIR} devdriver)
endif ()

# Qt Common
if (NOT QTCOMMON_DIR)
    FetchContent_Declare(
            qt_common
            GIT_REPOSITORY "https://github.com/GPUOpen-Tools/qt_common.git"
            GIT_TAG master
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/qt_common"
    )
    FetchContent_MakeAvailable(qt_common)
else ()
    add_subdirectory(${QTCOMMON_DIR} qt_common)
endif ()

# SPM DB
if (NOT SPM_DB_DIR)
    FetchContent_Declare(
            spm_db
            GIT_REPOSITORY "https://github.com/GPUOpen-Tools/spm_db.git"
            GIT_TAG main
            SOURCE_DIR "${PROJECT_SOURCE_DIR}/external/spm_db"
    )
    FetchContent_MakeAvailable(spm_db)
else ()
    add_subdirectory(${SPM_DB_DIR} spm_db)
endif ()

if (RDP_ENABLE_UNIT_TESTS)

     # Google test
     FetchContent_Declare(
        googletest
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "release-1.11.0"
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/third_party/googletest
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    set_target_properties(gmock gmock_main gtest gtest_main PROPERTIES FOLDER "External/ThirdParty/GoogleTest")

    # Light OpenCL SDK
    set(LIGHT_OCL_SDK_ARCHIVE "lightOCLSDK.zip")
    set(LIGHT_OCL_SDK_URL "https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/files/1406216/${LIGHT_OCL_SDK_ARCHIVE}")

    FetchContent_Declare(
            light_ocl_sdk
            URL ${LIGHT_OCL_SDK_URL}
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/third_party/light_ocl_sdk
    )
    FetchContent_MakeAvailable(light_ocl_sdk)
endif()

set(RDP_COMPLETED_FETCH_DEPENDENCIES TRUE)
