
#######################################################################################################################
### Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

add_executable(MatrixMulDispatchCounter)
set_target_properties(MatrixMulDispatchCounter PROPERTIES FOLDER "Test/OpenCL")
devtools_target_options(MatrixMulDispatchCounter)

target_include_directories(MatrixMulDispatchCounter
        PRIVATE
        inc
        ${OpenCL_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/source/api/wrapper
        ${PROJECT_SOURCE_DIR}/source/api
        )

target_sources(MatrixMulDispatchCounter PRIVATE
        src/matrixmul_host.cpp
        src/utils.cpp
        ${PROJECT_SOURCE_DIR}/source/api/wrapper/capture_api_wrapper.cpp
        )

set(API_TARGET_DIR $<TARGET_FILE_DIR:RdpCaptureApi>)

#Copy RdpCaptureApi and GPA Counters library to the output directory (where the executable ends up)
ADD_CUSTOM_COMMAND(TARGET MatrixMulDispatchCounter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "copying RdpCaptureApi.dll from ${API_TARGET_DIR} to $<TARGET_FILE_DIR:MatrixMulDispatchCounter>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${API_TARGET_DIR}/RdpCaptureApi.dll
        $<TARGET_FILE_DIR:MatrixMulDispatchCounter>
        COMMAND ${CMAKE_COMMAND} -E echo "copying $<TARGET_FILE:gpa_counters> to $<TARGET_FILE_DIR:MatrixMulDispatchCounter>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gpa_counters> $<TARGET_FILE_DIR:MatrixMulDispatchCounter>

        )

target_link_libraries(MatrixMulDispatchCounter RdpCaptureApi)
target_link_libraries(MatrixMulDispatchCounter ${OpenCL_LIBRARY})

install(TARGETS MatrixMulDispatchCounter
        RUNTIME DESTINATION . COMPONENT self_capture_apps
        BUNDLE DESTINATION . COMPONENT self_capture_apps)

install(FILES $<TARGET_FILE:gpa_counters> DESTINATION . COMPONENT self_capture_apps)
