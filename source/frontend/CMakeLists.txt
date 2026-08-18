#######################################################################################################################
### Copyright (c) 2019-2025 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

cmake_policy(SET CMP0087 NEW)
project(RadeonDeveloperPanel)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if (WIN32)
    set(RDP_WINDOWS_SRC rdp.rc)
endif (WIN32)

set(RDP_SOURCE
        navigation_tab_widget.h
        navigation_tab_widget.cpp
        navigation_tab_bar.h
        navigation_tab_bar.cpp
        settings_manager.cpp
        crash/crash_handler.cpp
        bug/bug_report_generator.cpp
        models/connection_model.cpp
        models/timeout_model.cpp
        timeout_dialog.cpp
        connection_widget.cpp
        bottom_bar_tab_widget.cpp
        capture_tab_widget.cpp
        system_tab_widget.cpp
        logging/logging_manager.cpp
        logging/logging_model.cpp
        logging/debug_log_widget.cpp
        logging/debug_log_tree_view.cpp
        models/system_info_model.cpp
        system_info_widget.cpp
        utilities.cpp
        main.cpp
        mainwindow.cpp
        application.cpp
        models/application_model.cpp
        models/application_proxy_model.cpp
        application_context_menu.cpp
        bottom_bar_splitter.cpp
        blocklist_item_delegate.cpp
        models/blocklist_model.cpp
        models/module_model.cpp
        models/preset_model.cpp
        no_workflow_settings_widget.cpp
        settings_widget.cpp
        models/api_model.cpp
        models/api_proxy_model.cpp
        sidebar_widget.cpp
        sidebar/add_application_dialog.cpp
        sidebar/tight_list_view.cpp
        sidebar/application_list_widget.cpp
        sidebar/blocklist_widget.cpp
        sidebar/executable_list_widget.cpp
        sidebar/available_modules/available_modules_widget.cpp
        sidebar/available_modules/available_modules_delegate.cpp
        presets/save_preset_dialog.cpp
        presets/std_preset_dialog.cpp

        models/sidebar/auto_connect_model.cpp
)

set(RDP_INCLUDE
        ../version.h
        settings_manager.h
        crash/crash_notify.h
        crash/crash_handler.h
        crash/crash_factory.h
        bug/bug_report.h
        bug/bug_report_generator.h
        models/connection_model.h
        models/timeout_model.h
        timeout_dialog.h
        connection_widget.h
        bottom_bar_tab_widget.h
        capture_tab_widget.h
        system_tab_widget.h
        logging/available_logging_filters_model.h
        logging/logging_manager.h
        logging/logging_model.h
        logging/debug_log_widget.h
        logging/debug_log_tree_view.h
        definitions.h
        models/system_info_model.h
        system_info_widget.h
        utilities.h
        application.h
        models/application_model.h
        models/application_proxy_model.h
        application_context_menu.h
        mainwindow.h
        tool_wrapper_types.h
        bottom_bar_splitter.h
        blocklist_item_delegate.h
        models/blocklist_model.h
        models/module_model.h
        models/preset_model.h
        no_workflow_settings_widget.h
        settings_widget.h
        models/api_model.h
        models/api_proxy_model.h
        sidebar_widget.h
        sidebar/add_application_dialog.h
        sidebar/tight_list_view.h
        sidebar/application_list_widget.h
        sidebar/blocklist_widget.h
        sidebar/executable_list_widget.h
        sidebar/available_modules/available_modules_widget.h
        sidebar/available_modules/available_modules_delegate.h
        presets/save_preset_dialog.h
        presets/std_preset_dialog.h

        models/sidebar/auto_connect_model.h
)

set(RDP_UI
        mainwindow.ui
        connection_widget.ui
        logging/debug_log_widget.ui
        settings_widget.ui
        system_info_widget.ui
        no_workflow_settings_widget.ui
        sidebar/executable_list_widget.ui
        presets/save_preset_dialog.ui
        presets/std_preset_dialog.ui
)

# Add sources for Windows crash handling
IF (WIN32)
    list(APPEND RDP_SOURCE crash/win/crash_notify_win.cpp crash/win/crash_handler_win.cpp)
    list(APPEND RDP_INCLUDE crash/win/crash_notify_win.h crash/win/crash_handler_win.h)
ENDIF ()

qt_add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
)
# Apply standard compilation options
devtools_target_options(${PROJECT_NAME})

# Set the RPATH properly on Linux
if (LINUX)
    set_target_properties(${PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
endif ()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION . COMPONENT apps
        BUNDLE DESTINATION . COMPONENT apps
        ARCHIVE DESTINATION lib/ COMPONENT apps
        LIBRARY DESTINATION lib/ COMPONENT apps)

if (WIN32)
    # Add dependency on EnableSyncPrimitives.exe utility application
    add_dependencies(${PROJECT_NAME} EnableSyncPrimitives)
endif ()

# GPA library - Use the library path exported by spm_db

if (WIN32)
    if (NOT CMAKE_BUILD_TYPE MATCHES "Release")
        install(FILES $<TARGET_PDB_FILE:${PROJECT_NAME}> DESTINATION . OPTIONAL COMPONENT apps)
    endif ()
    install(FILES $<TARGET_FILE:gpa_counters> DESTINATION . COMPONENT apps)
elseif (LINUX)
    install(FILES $<TARGET_FILE:gpa_counters> DESTINATION lib/ COMPONENT apps)
endif ()

# Post-Build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gpa_counters> $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/scripts $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
)
if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:EnableSyncPrimitives> $<TARGET_FILE_DIR:${PROJECT_NAME}>/scripts
    )
endif ()
if (LINUX)
    file(GLOB KERNEL_TRACING_LIBS
            ${EXTERNAL_DIR}/libtraceevent/lib/libtraceevent.so.1
            ${EXTERNAL_DIR}/libtracefs/lib/libtracefs.so.1)
    install(FILES ${KERNEL_TRACING_LIBS} DESTINATION lib/ COMPONENT apps)
endif ()

if (LINUX)
    # Ensure that wayland plugin integration is enabled for deployment to work
    qt_import_plugins(${PROJECT_NAME} INCLUDE Qt6::QWaylandIntegrationPlugin)
endif ()

# Deploy Qt binaries for build and install
deploy_qt(${PROJECT_NAME} apps)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/source)
target_include_directories(${PROJECT_NAME} PRIVATE
        ../ ../modules ../modules/common/inc ../api ${LIBDEVTRACE_DIR})
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${JSON_LIB_DIR})
if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
            ${RDP_SOURCE}
            ${RDP_UI}
            ${RDP_INCLUDE}
            ${RDP_WINDOWS_SRC}
            resources.qrc
    )
else ()
    target_sources(${PROJECT_NAME} PRIVATE
            ${RDP_SOURCE}
            ${RDP_UI}
            ${RDP_INCLUDE}
            resources.qrc
    )
endif (WIN32)

target_link_libraries(${PROJECT_NAME} PRIVATE
        general
        dd_tools
        ddRouter
        ddNet
        QtCustomWidgets
        QtUtils
        MercuryModuleExt
        system_info
        LibDevTrace
        DevToolsMemoryTraceModule
        DevToolsProfilingModule
        DevToolsRaytracingModule
        DevToolsCrashAnalysisModule
        DevToolsDeviceClocksModule
        SystemTraceModuleStatic
        ddRouterUtils
        ddSiphonModule
        Qt::Widgets
        Qt::Network
        debug Qt::Test
)

if (LINUX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads Qt::WaylandClient)
endif ()

source_group("Generated"
        ../version.h
)
