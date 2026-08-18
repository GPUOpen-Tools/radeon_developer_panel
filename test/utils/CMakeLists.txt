#######################################################################################################################
### Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
### \author AMD Developer Tools Team
#######################################################################################################################

add_library(TestUtils STATIC)

target_link_libraries(TestUtils
        PUBLIC
        LibDevTrace
        )

set_target_properties(TestUtils
        PROPERTIES
        FOLDER "TestUtils"
        )

target_include_directories(TestUtils
        PRIVATE
        ${LIBDEVTRACE_DIR}
        )
target_include_directories(TestUtils SYSTEM PRIVATE
        ${JSON_LIB_DIR}
        )

# Apply common devtools target options
devtools_target_options(TestUtils)

target_sources(TestUtils PRIVATE
        mock/mock_trace_io.h
        mock/mock_trace_io.cpp
        test_wait.h
        json_utils.h
        json_utils.cpp
        )

set_target_properties(TestUtils PROPERTIES FOLDER "Test")
