﻿Radeon™ Developer Panel
==========================

The Radeon Developer Panel is part of a suite of tools that can be used
by developers to optimize DirectX® 12, Vulkan®, OpenCL™ and HIP applications for AMD RDNA™
hardware. The suite is comprised of the following software:

-  **Radeon Developer Mode Driver** - This is shipped as part of the AMD
   public driver and supports the developer mode features
   required for profiling and debugging.

-  **Radeon Developer Service (RDS)** - A system tray application that
   unlocks the Developer Mode Driver features and supports
   communications with high level tools.

-  **Radeon Developer Service - CLI (Headless RDS)** - A console (i.e.
   non-GUI) application that unlocks the Developer Mode Driver features
   and supports communication with high level tools.

-  **Radeon Developer Panel (RDP)** - A GUI application that allows the
   developer to configure driver settings and generate profiles from
   DirectX12, Vulkan, OpenCL and HIP applications.

-  **Radeon GPU Profiler (RGP)** - A GUI tool used to visualize and
   analyze the profile data.

-  **Radeon GPU Detective (RGD)** - A command line tool that assists developers in post-mortem analysis of GPU crashes.

-  **Radeon Memory Visualizer (RMV)** - A GUI tool used to visualize and analyze
   the memory trace data.

-  **Radeon Raytracing Analyzer (RRA)** - A GUI tool used to visualize and analyze
   the raytracing data.

   This document describes how the Radeon Developer Panel can be used to capture
   a profile, memory trace or a raytracing scene for an application on AMD RDNA graphics hardware. The
   Radeon Developer Panel connects to the Radeon Developer Service in
   order to collect a profile, trace or scene.

   **RGP documentation:** https://gpuopen.com/manuals/rgp_manual/rgp_manual-index/

   **RMV documentation:** https://gpuopen.com/manuals/rmv_manual/rmv_manual-index/

   **RRA documentation:** https://gpuopen.com/manuals/rra_manual/rra_manual-index/

   **RGD documentation:** https://gpuopen.com/manuals/rgd_manual/rgd_manual-index/

.. NOTE::
   By default, the driver allocates a maximum of 75 MB video
   memory per Shader Engine to capture RGP profiles. The driver allocates
   300 MB video memory for the single shader engine with instruction tracing enabled.
   This can be configured in the capture settings.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   initial_setup.rst
   remote_connections.rst
   capture.rst
   features.rst
   settings.rst
   bug_report.rst
   radeon_developer_service
   known_issues.rst

Graphics APIs, RDNA hardware, and operating systems
---------------------------------------------------------------------

**Supported APIs**

-  DirectX12

-  Vulkan

\ **Supported RDNA hardware**

-  AMD Radeon RX 9000 series

-  AMD Radeon RX 7000 series

-  AMD Radeon RX 6000 series

-  AMD Radeon RX 5000 series

-  AMD Ryzen™ Processors with Radeon Graphics

\ **Supported Operating Systems**

-  Windows® 10

-  Windows® 11

-  Ubuntu 22.04 LTS (Vulkan only)

-  Ubuntu 24.04 LTS (Vulkan only)

Compute APIs, RDNA hardware, and operating systems
--------------------------------------------------------------------

**Supported APIs**

-  OpenCL

-  HIP

\ **Supported RDNA hardware**

-  AMD Radeon RX 9000 series

-  AMD Radeon RX 7000 series

-  AMD Radeon RX 6000 series

-  AMD Radeon RX 5000 series

-  AMD Ryzen Processors with Radeon Graphics

\ **Supported Operating Systems**

-  Windows® 10

-  Windows® 11
