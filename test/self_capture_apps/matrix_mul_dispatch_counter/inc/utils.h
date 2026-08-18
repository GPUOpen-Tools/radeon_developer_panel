// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

void randomMemInit(float* data, int size);

long LoadOpenCLKernel(char const* path, char** buf);
