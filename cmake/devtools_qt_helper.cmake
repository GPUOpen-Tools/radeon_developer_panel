# Copyright Advanced Micro Devices, Inc. All rights reserved.

### @author AMD Developer Tools Team

cmake_minimum_required(VERSION 3.10)

# Attempt to automatically find Qt on the local machine
if (LINUX)
    find_package(Qt6 QUIET COMPONENTS REQUIRED Core Widgets Network Gui Test WaylandClient)
else ()
    find_package(Qt6 QUIET COMPONENTS Core Widgets Network Gui Test)
endif ()

if (Qt6_DIR)
    message(STATUS "Qt6 cmake package found at ${Qt6_DIR}")
endif ()

if (Qt6_DIR)
    #######################################################################################################################
    # Setup the INSTALL target to include Qt DLLs
    ##
    # Logic used to find and use Qt's internal deployment tool to copy Qt-based dependencies and DLLs upon building, so
    # that we can run the application from an IDE and so that we just have a simple build directory with all dependencies
    # already deployed that we can easily distribute
    #######################################################################################################################
    get_target_property(_qmake_executable Qt::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)

    # Determine if Qt was found via CMAKE_PREFIX_PATH (bundled/fetched Qt) rather than the system.
    # This is used to conditionally run patchelf for ICU library deployment.
    set(_QT_FROM_PREFIX_PATH FALSE)
    if (LINUX AND CMAKE_PREFIX_PATH)
        foreach(_prefix_path ${CMAKE_PREFIX_PATH})
            file(REAL_PATH "${_prefix_path}" _resolved_prefix)
            file(REAL_PATH "${Qt6_DIR}" _resolved_qt_dir)
            string(FIND "${_resolved_qt_dir}" "${_resolved_prefix}" _pos)
            if (_pos EQUAL 0)
                set(_QT_FROM_PREFIX_PATH TRUE)
                break()
            endif ()
        endforeach()
    endif ()

    function(deploy_qt_build target)
        set(DEPLOY_QT_HELPER_EXE $<TARGET_FILE:DeployQtHelper>)

        if (WIN32)
            find_program(DEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
            set(DEPLOYQT_POST_BUILD_COMMAND
                    ${DEPLOY_QT_HELPER_EXE} ${DEPLOYQT_EXECUTABLE} $<TARGET_FILE:${target}> -verbose 0 --no-compiler-runtime --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-network
                    WORKING_DIRECTORY ${PROJECT_BINARY_DIR})

            # Adds a lockfile and makes sure the locking utility is built
            add_dependencies(${target} DeployQtHelper)
            file(TOUCH "${PROJECT_BINARY_DIR}/deploy_qt.lock")

            # Deploy Qt to build directory after a successful build
            add_custom_command(
                    TARGET ${target} POST_BUILD
                    COMMAND ${DEPLOYQT_POST_BUILD_COMMAND}
            )
        endif ()

        if (LINUX AND _QT_FROM_PREFIX_PATH)
            # Ensure that libicudata.so.70 is added as an explicit dependency of Qt based targets so that it gets deployed.
            # This is only needed when using a bundled Qt from CMAKE_PREFIX_PATH, not the system Qt.
            add_custom_command(TARGET ${target} POST_BUILD COMMAND patchelf --add-needed libicudata.so.70 $<TARGET_FILE:${target}>)
        endif ()

    endfunction()

    function(deploy_qt target component)
        set(target_file_dir "$<TARGET_FILE_DIR:${target}>")
        set(target_file_name "$<TARGET_FILE_NAME:${target}>")

        # Handle build-time deployment
        deploy_qt_build(${target})

        # Handle install deployment

        set(EXECUTABLE_PATH "\${QT_DEPLOY_BIN_DIR}/${target_file_name}")
        if (WIN32)
            set(CUSTOM_DEPLOY_TOOL_OPTIONS "--no-compiler-runtime --no-translations --no-system-d3d-compiler --no-system-dxc-compiler --no-opengl-sw --no-network")
            # Generate deployment script of Qt binaries and plugins
            qt_generate_deploy_script(TARGET ${target}
                    OUTPUT_SCRIPT deploy_script
                    CONTENT "
                    qt6_deploy_runtime_dependencies(
                            EXECUTABLE \"${target_file_name}\"
                            NO_TRANSLATIONS
                            DEPLOY_TOOL_OPTIONS ${CUSTOM_DEPLOY_TOOL_OPTIONS}
                            BIN_DIR .
                            LIB_DIR .
                    )")
        else()
            # Generate deployment script of Qt binaries and plugins
            qt_generate_deploy_app_script(TARGET ${target}
                OUTPUT_SCRIPT deploy_script
                NO_UNSUPPORTED_PLATFORM_ERROR
                NO_TRANSLATIONS
            )
        endif()

        install(SCRIPT ${deploy_script} COMPONENT ${component})
    endfunction()
endif ()
