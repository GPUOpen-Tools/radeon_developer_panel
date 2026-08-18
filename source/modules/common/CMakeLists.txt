#######################################################################################################################
### Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

set(DEVTOOLS_MODULE_FOLDER "DevTools Modules/Common")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS src)

###Common Shared Code ####################################################################################################
add_library(DevToolsCommonStatic STATIC)
devtools_target_options(DevToolsCommonStatic)
target_link_libraries(DevToolsCommonStatic
        PUBLIC
        amdrdf
        PRIVATE
        ddNet
        ddRouter
        dd_tools
        MercuryModuleExt
        QtCustomWidgets
        QtUtils
        Qt::Widgets
        LibDevTrace
)
if (LINUX)
    target_link_libraries(DevToolsCommonStatic
            PUBLIC
            rt)
endif ()
set_target_properties(DevToolsCommonStatic
        PROPERTIES
        FOLDER ${DEVTOOLS_MODULE_FOLDER}
)

target_include_directories(DevToolsCommonStatic PRIVATE inc src ${LIBDEVTRACE_DIR})
target_include_directories(DevToolsCommonStatic SYSTEM PRIVATE ${JSON_LIB_DIR})

if (LINUX)
    target_compile_options(DevToolsCommonStatic PUBLIC -fexceptions -frtti)
endif ()

if (WIN32)
    # Add any Windows platform specific source
elseif (LINUX)
    set(DEVTOOLS_COMMON_PLATFORM_SRC
            inc/linux/signal_handler.h
            src/linux/signal_handler.cpp)
endif ()

if (DISABLE_KEYBOARD_HANDLER_IN_DEBUGGER_WIN)
    add_compile_definitions(DevToolsCommonStatic DISABLE_KEYBOARD_HANDLER_IN_DEBUGGER_WIN=1)
endif ()

target_sources(DevToolsCommonStatic PRIVATE
        inc/api/dev_tools_logging.h
        inc/api/dev_tools_module.h
        inc/capture_delay_widget.h
        inc/capture_progress_widget.h
        inc/collapsible_pane.h
        inc/recent_trace_model.h
        inc/show_selection_no_focus_delegate.h
        inc/rdf_file_dialog.h
        inc/rdp_combo_box.h
        inc/rdp_spin_box.h
        inc/logger.h
        inc/util.h
        inc/definitions.h
        inc/global_shortcut_edit.h
        inc/global_shortcut_manager.h
        inc/collapsible_pane_button.h
        inc/named_mutex.h
        inc/single_instance.h
        inc/single_application_instance.h
        inc/delay_store.h
        inc/formatting.h
        inc/input_validation.h
        inc/file_client_view.h
        inc/focus_notification_line_edit.h
        inc/no_focus_delegate.h
        inc/utility_view.h
        inc/utility_view_v2.h
        inc/model/qt_trace_io.h
        inc/model/qt_trace_timer.h
        inc/model/trace_file_opener.h
        inc/model/trace_source_view_model.h
        inc/model/recent_trace_model_base.h
        inc/model/utility/base_userdata_view_model.h
        inc/model/utility/userdata_view_model.h
        inc/model/current_connection_model.h
        inc/model/file_utils.h
        inc/model/prelaunch_settings_helper.h
        inc/model/rdf_file_model.h
        inc/model/rdf_file_proxy_model.h
        inc/view/model_binder.h
        inc/view/min_row_count_tree_view.h
        inc/view/split_client_view.h
        inc/view/output_path_row_view.h
        inc/view/scrollable_pane.h
        inc/view/base_utility_view.h
        inc/view/base_utility_view.h
        src/api/dev_tools_module.cpp
        src/capture_delay_widget.cpp
        src/global_shortcut_manager.cpp
        src/collapsible_pane_button.cpp
        src/utility_view.cpp
        src/utility_view_v2.cpp
        src/global_shortcut_edit.cpp
        src/single_application_instance.cpp
        src/named_mutex.cpp
        src/recent_trace_model.cpp
        src/show_selection_no_focus_delegate.cpp
        src/logger.cpp
        src/util.cpp
        src/formatting.cpp
        src/input_validation.cpp
        src/file_client_view.cpp
        src/rdf_file_dialog.cpp
        src/rdp_combo_box.cpp
        src/rdp_spin_box.cpp
        src/focus_notification_line_edit.cpp
        src/capture_progress_widget.cpp
        src/collapsible_pane.cpp
        src/model/trace_source_view_model.cpp
        src/model/recent_trace_model_base.cpp
        src/model/utility/userdata_view_model.cpp
        src/model/mercury_logger.cpp
        src/model/qt_trace_io.cpp
        src/model/qt_trace_timer.cpp
        src/model/trace_file_opener.cpp
        src/model/current_connection_model.cpp
        src/model/file_utils.cpp
        src/model/rdf_file_model.cpp
        src/model/rdf_file_proxy_model.cpp
        src/model/prelaunch_settings_helper.cpp
        src/view/model_binder.cpp
        src/view/min_row_count_tree_view.cpp
        src/view/split_client_view.cpp
        src/view/output_path_row_view.cpp
        src/view/scrollable_pane.cpp
        src/view/base_source_utility_view.cpp
        ${DEVTOOLS_COMMON_PLATFORM_SRC})

if (WIN32)
    target_sources(DevToolsCommonStatic PRIVATE
            inc/system_keyboard_hook.h
            src/system_keyboard_hook.cpp
    )
endif ()
