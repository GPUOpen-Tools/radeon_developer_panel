#=============================================================================
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# @author AMD Developer Tools Team
#=============================================================================

add_library(base STATIC
        camera.hpp
        camera.cpp
        keyboard.hpp
        keyboard.cpp
        mouse.hpp
        mouse.cpp
        game_timer.cpp
        d3d12_context.cpp
        example.hpp
        example.cpp
        file.cpp)

target_include_directories(base PUBLIC .)
target_compile_options(base PUBLIC /utf-8)
target_link_libraries(base PUBLIC
        fmt::fmt
        SDL3::SDL3
        Microsoft::DirectX-Headers
        Microsoft::DirectX-Guids
        Microsoft::DirectX12-Agility
        Microsoft::DirectXTK12
        Microsoft::DirectStorage
        imgui::imgui
        dxgi
        dxguid
        runtimeobject)
