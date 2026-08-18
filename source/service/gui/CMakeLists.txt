#######################################################################################################################
### Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

cmake_policy(SET CMP0087 NEW)
project(RadeonDeveloperService)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(WINDOWS_SOURCES
        "../../assets/rds_icon_levanim.ico"
        "../win/RDS.rc"
        "../win/resource.h"
)

set(RDS_SOURCES
        "settings.h"
        "settings.cpp"
        "configuration_window.cpp"
        "configuration_window.ui"
        "configuration_window.h"
        "main_window.cpp"
        "main_window.ui"
        "main_window.h"
        "resources.qrc"
        "main.cpp"
        "definitions.h"
)

IF (WIN32)
    qt_add_executable(${PROJECT_NAME})

    set_target_properties(${PROJECT_NAME} PROPERTIES
            WIN32_EXECUTABLE TRUE
            COMPILE_FLAGS "-DRDS_LOG_TO_PANEL"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${RDS_SOURCES} ${WINDOWS_SOURCES})
ELSEIF (LINUX)
    qt_add_executable(${PROJECT_NAME})
    target_sources(${PROJECT_NAME} PRIVATE ${RDS_SOURCES} ${LINUX_SOURCES})
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-DRDS_LOG_TO_PANEL")
ENDIF (WIN32)
target_include_directories(${PROJECT_NAME} PRIVATE ../../ ../../modules ${FMT_LIB_DIR}/include)

# Apply standard compilation options
devtools_target_options(${PROJECT_NAME})

IF (WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets QtCustomWidgets
            QtUtils tdh.lib ddRouter DevToolsCommonStatic SystemTraceModuleStatic ddRouterUtils ddSiphonModule)
ELSEIF (LINUX)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets QtCustomWidgets
            QtUtils pthread ddRouter DevToolsCommonStatic SystemTraceModuleStatic ddRouterUtils ddSiphonModule)
ENDIF ()

if (LINUX)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
endif ()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION . COMPONENT apps
        BUNDLE DESTINATION . COMPONENT apps
        ARCHIVE DESTINATION lib/ COMPONENT apps
        LIBRARY DESTINATION lib/ COMPONENT apps)

if (LINUX)
    # Ensure that wayland plugin integration is enabled for deployment to work
    qt_import_plugins(${PROJECT_NAME} INCLUDE Qt6::QWaylandIntegrationPlugin)
endif ()

# Deploy Qt binaries for build and install
deploy_qt(${PROJECT_NAME} apps)

