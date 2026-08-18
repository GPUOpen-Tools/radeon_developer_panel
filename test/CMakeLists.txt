#######################################################################################################################
### Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

if (RDP_ENABLE_UNIT_TESTS)
set(OpenCL_INCLUDE_DIRS "${EXTERNAL_DIR}/third_party/light_ocl_sdk/include")
set(OpenCL_LIBRARY "${EXTERNAL_DIR}/third_party/light_ocl_sdk/lib/x86_64/OpenCL.lib")

    add_compile_definitions(SAMPLE_APP_BINARY_DIR="./sample_apps")

    include(GoogleTest)

    add_subdirectory(utils)
    set(TEST_UTIL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/utils")

    if (RDP_ENABLE_UNIT_TESTS)
        add_subdirectory(unit)
    endif ()

    if (WIN32)
    	add_subdirectory(self_capture_apps)
    endif()
endif ()
