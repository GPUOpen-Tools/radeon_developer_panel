#######################################################################################################################
### Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(WINDOWS_SOURCES
        "win/EnableSyncPrimitives.rc"
        "win/resource.h"
)

add_executable(EnableSyncPrimitives main.cpp ${WINDOWS_SOURCES})
target_link_libraries(EnableSyncPrimitives PRIVATE
        ddGpuInfo
)
set_target_properties(EnableSyncPrimitives PROPERTIES CXX_STANDARD 20 COMPILE_FLAGS "-DHEADLESS -DDEVDRIVER_PRINT_TO_CONSOLE -DDEVDRIVER_LOG_LEVEL=LogLevel::Debug")
target_include_directories(EnableSyncPrimitives PRIVATE ../)
install(TARGETS EnableSyncPrimitives
        RUNTIME DESTINATION scripts COMPONENT apps
        ARCHIVE DESTINATION lib COMPONENT apps
        LIBRARY DESTINATION lib COMPONENT apps)
install(TARGETS EnableSyncPrimitives
        RUNTIME DESTINATION scripts COMPONENT self_capture_apps
        ARCHIVE DESTINATION lib COMPONENT self_capture_apps
        LIBRARY DESTINATION lib COMPONENT self_capture_apps)
