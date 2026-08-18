#=============================================================================
# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
# @author AMD Developer Tools Team
#=============================================================================

cmake_minimum_required(VERSION 3.24)
project(D3D12)

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(directx12-agility CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(dstorage CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# RDP Capture API. This is not a vcpkg port; point CMake at the installed package
# with -DCMAKE_PREFIX_PATH=<rdp-install-prefix> (or -DRdpCaptureApi_DIR=<prefix>/lib/cmake/RdpCaptureApi).
find_package(RdpCaptureApi CONFIG REQUIRED)

include(CompileShaders)
file(GLOB_RECURSE HLSL_SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.hlsl)
CompileHLSLShaders("${HLSL_SHADERS}")

# Examples
add_subdirectory(source)

