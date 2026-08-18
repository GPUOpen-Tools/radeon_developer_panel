//=============================================================================
// Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
//=============================================================================

#pragma once

#include <cstdint>

#ifdef __APPLE__
#ifdef __arm__
#define _XM_ARM_NEON_INTRINSICS_  // Apple architecture
#endif
#endif

#include <DirectXColors.h>
#include <DirectXMath.h>

#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;
