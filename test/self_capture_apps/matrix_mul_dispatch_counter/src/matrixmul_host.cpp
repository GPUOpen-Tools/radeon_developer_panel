// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

////////////////////////////////////////////////////////////////////////////////
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <CL/cl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

#ifdef _WIN32
#include "windows.h"
#endif

#include "capture_api_wrapper.h"

////////////////////////////////////////////////////////////////////////////////
#define WA 512
#define HA 512
#define WB 512

#define HB WA
#define WC WB
#define HC HA

#define MAX_DISPATCH_COUNT (500)
static const char* kOptionIterations = "--iterations";
static const char* kOptionQuiet      = "--quiet";
static const char* kOptionDevice     = "--device";

////////////////////////////////////////////////////////////////////////////////

void RunDispatch(cl_context context, cl_command_queue commands, cl_kernel kernel, bool quiet)
{
    CaptureWrapper::Get().PreDispatch();

    int err;  // error code returned from api calls

    // OpenCL device memory for matrices
    cl_mem d_A;
    cl_mem d_B;
    cl_mem d_C;

    // set seed for rand()
    srand(2014);

    //Allocate host memory for matrices A and B
    unsigned int size_A     = WA * HA;
    unsigned int mem_size_A = sizeof(float) * size_A;
    float*       h_A        = (float*)malloc(mem_size_A);

    unsigned int size_B     = WB * HB;
    unsigned int mem_size_B = sizeof(float) * size_B;
    float*       h_B        = (float*)malloc(mem_size_B);

    //Initialize host memory
    randomMemInit(h_A, size_A);
    randomMemInit(h_B, size_B);

    //Allocate host memory for the result C
    unsigned int size_C     = WC * HC;
    unsigned int mem_size_C = sizeof(float) * size_C;
    float*       h_C        = (float*)malloc(mem_size_C);

    // Create the input and output arrays in device memory for our calculation
    d_C = clCreateBuffer(context, CL_MEM_READ_WRITE, mem_size_A, NULL, &err);
    d_A = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, mem_size_A, h_A, &err);
    d_B = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, mem_size_B, h_B, &err);

    if (!d_A || !d_B || !d_C)
    {
        CaptureWrapper::Get().Log("Error: Failed to allocate device memory!\n");
        exit(1);
    }

    static int dispatchCount = 0;
    if (!quiet)
    {
        CaptureWrapper::Get().Log("Dispatch %d matrix multiplication for matrices A (%dx%d) and B (%dx%d) ...\n", dispatchCount, WA, HA, WB, HB);
    }
    ++dispatchCount;

    //Launch OpenCL kernel
    size_t localWorkSize[2], globalWorkSize[2];

    int wA = WA;
    int wC = WC;
    err    = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void*)&d_C);
    err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), (void*)&d_A);
    err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&d_B);
    err |= clSetKernelArg(kernel, 3, sizeof(int), (void*)&wA);
    err |= clSetKernelArg(kernel, 4, sizeof(int), (void*)&wC);

    if (err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to set kernel arguments! %d\n", err);
        exit(1);
    }

    localWorkSize[0]  = 16;
    localWorkSize[1]  = 16;
    globalWorkSize[0] = WA;
    globalWorkSize[1] = HA;

    err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);

    if (err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to execute kernel! %d\n", err);
        exit(1);
    }

    err = clFlush(commands);

    //Retrieve result from device
    err = clEnqueueReadBuffer(commands, d_C, CL_TRUE, 0, mem_size_C, h_C, 0, NULL, NULL);

    if (err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to read output array! %d\n", err);
        exit(1);
    }

    //print out the results
    /*
    printf("\n\nMatrix C (Results)\n");
    int i;
    for(i = 0; i < size_C; i++)
    {
    printf("%f ", h_C[i]);
    if(((i + 1) % WC) == 0)
    printf("\n");
    }
    printf("\n");
    */

    //Shutdown and cleanup
    free(h_A);
    free(h_B);
    free(h_C);

    clReleaseMemObject(d_A);
    clReleaseMemObject(d_C);
    clReleaseMemObject(d_B);

    if (!quiet)
    {
        CaptureWrapper::Get().Log("Matrix multiplication completed...\n");
    }
}

int main(int argc, char** argv)
{
    int err;  // error code returned from api calls
    CaptureWrapper::Get().ProcessCommandLine(argc, argv);

    unsigned int max_dispatch_count            = MAX_DISPATCH_COUNT;  // default for if option is not specified.
    unsigned int specified_device_id           = 0;                   // default for if option is not specified.
    bool         quiet                         = false;
    bool         read_iterations_from_next_arg = false;
    bool         read_device_from_next_arg     = false;
    bool         device_specified              = false;

    for (int count = 0; count < argc; count++)
    {
        const char* argument = argv[count];
        if (strstr(argument, kOptionIterations) != nullptr)
        {
            read_iterations_from_next_arg = true;
            continue;
        }
        if (strstr(argument, kOptionDevice) != nullptr)
        {
            read_device_from_next_arg = true;
            continue;
        }
        if (read_iterations_from_next_arg)
        {
            read_iterations_from_next_arg = false;
            if (argument != nullptr)
            {
                sscanf(argument, "%d", &max_dispatch_count);
            }
            continue;
        }
        if (read_device_from_next_arg)
        {
            read_device_from_next_arg = false;
            if (argument != nullptr)
            {
                sscanf(argument, "%d", &specified_device_id);
                device_specified = true;
            }
            continue;
        }
        if (strstr(argument, kOptionQuiet) != nullptr)
        {
            quiet = true;
        }
    }

    cl_device_id     device_id;  // compute device id
    cl_context       context;    // compute context
    cl_command_queue commands;   // compute command queue
    cl_program       program;    // compute program
    cl_kernel        kernel;     // compute kernel
    bool             dev_driver_init = CaptureWrapper::Get().Init();
    if (!dev_driver_init)
    {
        exit(-1);
    }

    if (!quiet)
    {
        CaptureWrapper::Get().Log("Initializing OpenCL device...\n");
    }

    cl_uint plat_cnt = 0;
    clGetPlatformIDs(0, 0, &plat_cnt);
    if (plat_cnt > 100)
    {
        CaptureWrapper::Get().Log("Warning: More than 100 OpenCL platforms detected. Only the first 100 will be used.\n");
        plat_cnt = 100;
    }

    cl_platform_id platform_ids[100];
    clGetPlatformIDs(plat_cnt, platform_ids, NULL);

    // Connect to a compute device
    int     gpu     = 1;
    cl_uint dev_cnt = 0;
    err             = clGetDeviceIDs(platform_ids[0], gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, 0, NULL, &dev_cnt);
    if (err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to create a device group!\n");
        return EXIT_FAILURE;
    }
    if (dev_cnt > 100)
    {
        CaptureWrapper::Get().Log("Warning: More than 100 OpenCL devices detected. Only the first 100 will be used.\n");
        dev_cnt = 100;
    }

    cl_device_id device_ids[100];
    err = clGetDeviceIDs(platform_ids[0], gpu ? CL_DEVICE_TYPE_GPU : CL_DEVICE_TYPE_CPU, dev_cnt, &device_ids[0], NULL);
    if (err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to create a device group!\n");
        return EXIT_FAILURE;
    }

    cl_uint dev_id_to_use = 0;
    if (device_specified)
    {
        dev_id_to_use = specified_device_id;
        if (dev_id_to_use >= dev_cnt)
        {
            CaptureWrapper::Get().Log("Error: Specified device ID is out of range! Defaulting to device 0.\n");
            dev_id_to_use = 0;
        }
    }
    else
    {
        cl_uint max_compute_units = 0;
        for (cl_uint i = 0; i < dev_cnt; ++i)
        {
            // Output device name.
            char device_name[256] = {0};
            err                   = clGetDeviceInfo(device_ids[i], CL_DEVICE_NAME, sizeof(device_name), device_name, NULL);
            if (err != CL_SUCCESS)
            {
                CaptureWrapper::Get().Log("Error: Failed to get device name!\n");
                return EXIT_FAILURE;
            }
            CaptureWrapper::Get().Log("Device %d: Name %s\n", i, device_name);

            // Output device vendor.
            cl_uint device_vendor = 0;
            err                   = clGetDeviceInfo(device_ids[i], CL_DEVICE_VENDOR_ID, sizeof(device_vendor), &device_vendor, NULL);
            if (err != CL_SUCCESS)
            {
                CaptureWrapper::Get().Log("Error: Failed to get device vendor!\n");
                return EXIT_FAILURE;
            }
            if (dev_cnt > 1)
            {
                CaptureWrapper::Get().Log("Device %d: Vendor %u\n", i, device_vendor);
            }
            if (device_vendor == 0x1002)  // AMD vendor ID
            {
                // Output device compute units.
                cl_uint max_cus = 0;
                err             = clGetDeviceInfo(device_ids[i], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(max_cus), &max_cus, NULL);
                if (err != CL_SUCCESS)
                {
                    CaptureWrapper::Get().Log("Error: Failed to get device compute units!\n");
                    return EXIT_FAILURE;
                }
                if (dev_cnt > 1)
                {
                    CaptureWrapper::Get().Log("Device %d: Compute Units %u\n", i, max_cus);
                }
                if (max_cus > max_compute_units)
                {
                    max_compute_units = max_cus;
                    dev_id_to_use     = i;
                }
            }
        }
    }
    if (dev_cnt > 1 || device_specified)
    {
        CaptureWrapper::Get().Log("Using Device %d\n", dev_id_to_use);
    }
    device_id = device_ids[dev_id_to_use];
    // Create a compute context
    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (!context)
    {
        CaptureWrapper::Get().Log("Error: Failed to create a compute context!\n");
        return EXIT_FAILURE;
    }

    // Create a command commands
    commands = clCreateCommandQueueWithProperties(context, device_id, 0, &err);
    if (!commands)
    {
        CaptureWrapper::Get().Log("Error: Failed to create a command commands!\n");
        return EXIT_FAILURE;
    }

    // Create the compute program from the kernel source
    const char* KernelSource =
        "__kernel void matrixMul(__global float* C, __global float* A, __global float* B, int wA, int wB)"
        "{"
        "    int tx = get_global_id(0);"
        "    int ty = get_global_id(1);"
        ""
        "    /* value stores the element that is*/"
        "    /* computed by the thread */"
        "    float value = 0;"
        "    for (int k = 0; k < wA; ++k)"
        "    {"
        "        float elementA = A[ty * wA + k];"
        "        float elementB = B[k * wB + tx];"
        "        value += elementA * elementB;"
        "    }"
        ""
        "    /* Write the matrix to device memory each */"
        "    /* thread writes one element */"
        "    C[ty * wA + tx] = value;"
        "}";

    program = clCreateProgramWithSource(context, 1, (const char**)&KernelSource, NULL, &err);
    if (!program)
    {
        CaptureWrapper::Get().Log("Error: Failed to create compute program!\n");
        return EXIT_FAILURE;
    }

    // Build the program executable
    err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        size_t len;
        char   buffer[2048];
        CaptureWrapper::Get().Log("Error: Failed to build program executable!\n");
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        CaptureWrapper::Get().Log(buffer);
        exit(1);
    }

    // Create the compute kernel in the program we wish to run
    //
    kernel = clCreateKernel(program, "matrixMul", &err);
    if (!kernel || err != CL_SUCCESS)
    {
        CaptureWrapper::Get().Log("Error: Failed to create compute kernel!\n");
        exit(1);
    }

    for (unsigned int i = 0; i < max_dispatch_count; ++i)
    {
        RunDispatch(context, commands, kernel, quiet);
    }

    CaptureWrapper::Get().WaitUntilFinished();
    CaptureWrapper::Get().Close();

    clReleaseProgram(program);
    clReleaseKernel(kernel);
    clReleaseCommandQueue(commands);
    clReleaseContext(context);

    return 0;
}
