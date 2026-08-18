// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "utils.h"
#include <CL/cl.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// Allocates a matrix with random float entries.
void randomMemInit(float* data, int size)
{
    int i;

    for (i = 0; i < size; ++i)
        data[i] = rand() / (float)RAND_MAX;
}

long LoadOpenCLKernel(char const* path, char** buf)
{
    FILE*  fp;
    size_t fsz;
    long   off_end;
    int    rc;

    /* Open the file */
    fp = fopen(path, "rb");
    if (NULL == fp)
    {
        return -1L;
    }

    /* Seek to the end of the file */
    rc = fseek(fp, 0L, SEEK_END);
    if (0 != rc)
    {
        return -1L;
    }

    /* Byte offset to the end of the file (size) */
    if (0 > (off_end = ftell(fp)))
    {
        return -1L;
    }
    fsz = (size_t)off_end;

    /* Allocate a buffer to hold the whole file */
    *buf = (char*)malloc(fsz + 1);
    if (NULL == *buf)
    {
        return -1L;
    }

    /* Rewind file pointer to start of file */
    rewind(fp);

    /* Slurp file into buffer */
    if (fsz != fread(*buf, 1, fsz, fp))
    {
        free(*buf);
        return -1L;
    }

    /* Close the file */
    if (EOF == fclose(fp))
    {
        free(*buf);
        return -1L;
    }

    /* Make sure the buffer is NUL-terminated, just in case */
    (*buf)[fsz] = '\0';

    /* Return the file size */
    return (long)fsz;
}
