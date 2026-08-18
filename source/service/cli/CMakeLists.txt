#######################################################################################################################
### Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

cmake_policy(SET CMP0087 NEW)
project(RadeonDeveloperServiceCLI)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(WINDOWS_SOURCES
        "../../assets/rds_icon_levanim.ico"
        "../win/RDS.rc"
        "../win/resource.h"
)

set(HEADLESS_SOURCES
        "command_line_parser.cpp"
        "command_line_parser.h"
        "command_line_parameter.cpp"
        "command_line_parameter.h"
        "cli_main.cpp"
        "definitions.h"
)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE ${HEADLESS_SOURCES})
if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE ${WINDOWS_SOURCES})
elseif (LINUX)
    target_sources(${PROJECT_NAME} PRIVATE ${LINUX_SOURCES})
endif ()
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-DHEADLESS -DDEVDRIVER_PRINT_TO_CONSOLE -DDEVDRIVER_LOG_LEVEL=LogLevel::Debug")
target_include_directories(${PROJECT_NAME} PRIVATE ../../ ../../modules)
# Apply standard compilation options
devtools_target_options(${PROJECT_NAME})

if (LINUX)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
endif ()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION . COMPONENT apps
        BUNDLE DESTINATION . COMPONENT apps
        ARCHIVE DESTINATION lib/ COMPONENT apps
        LIBRARY DESTINATION lib/ COMPONENT apps)

IF (WIN32)
    target_link_libraries(${PROJECT_NAME} tdh.lib ddRouter SystemTraceModuleStatic ddRouterUtils ddSiphonModule)
ELSEIF (LINUX)
    target_link_libraries(${PROJECT_NAME} ddRouter SystemTraceModuleStatic ddRouterUtils ddSiphonModule)
ENDIF ()

