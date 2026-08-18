#=============================================================================
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# @author AMD Developer Tools Team
#=============================================================================

set(EXAMPLE helloworld)

set(RESOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/SimpleShaderVS.bin)
list(APPEND RESOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/SimpleShaderPS.bin)

add_executable(${EXAMPLE} WIN32
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${RESOURCE_FILES})

# Ensure shader library is available
add_dependencies(${EXAMPLE} CompileHLSLShaders)

set_target_properties(${EXAMPLE}
        PROPERTIES
        RESOURCE "${RESOURCE_FILES}")

target_link_libraries(${EXAMPLE} PRIVATE base)

# RDP Capture API. The imported target supplies the public header include path
# (including the generated rdp_capture_api_export.h) and the import library. The
# generated export header marks RdpCaptureGetFnTable as dllimport for consumers.
target_link_libraries(${EXAMPLE} PRIVATE RdpCaptureApi::RdpCaptureApi)

# Copy RdpCaptureApi.dll next to the exe so the sample can run from the build tree.
# Its own dependencies (e.g. gpa_counters.dll, blocklist.ini) must also be present.
add_custom_command(TARGET ${EXAMPLE} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:RdpCaptureApi::RdpCaptureApi> $<TARGET_FILE_DIR:${EXAMPLE}>)

if(TARGET Microsoft::DirectX12-Agility)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/D3D12")
    add_custom_command(TARGET ${EXAMPLE} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Core,IMPORTED_LOCATION_RELEASE> $<TARGET_FILE_DIR:${EXAMPLE}>/D3D12
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PROPERTY:Microsoft::DirectX12-Layers,IMPORTED_LOCATION_DEBUG> $<TARGET_FILE_DIR:${EXAMPLE}>/D3D12
            COMMAND_EXPAND_LISTS
    )
endif()

