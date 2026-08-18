# RDP Capture API User Guide

The **RDP Capture API** lets an application drive the AMD Developer Mode driver
directly, the same way the Radeon Developer Panel (RDP) does. With it you can
capture RGP profiles, RMV traces, RRA scenes, and RGD crash dumps.

Everything in the API is exposed through the single public C header
[`RdpCaptureApi.h`](@ref RdpCaptureApi.h). The interface is plain C, so it can be
consumed from C, C++, or any language with a C FFI.

---

## Key concepts

| Concept | Type | Description |
|---|---|---|
| Function table | `RdpCaptureFnTable` | All API calls are made through function pointers in this table. |
| Context | `RdpCaptureContext` | An opaque handle representing one capture session. Only **one** context may exist per system. |
| Feature | `RdpCaptureFeature` | A capability you enable on a context: Profiling, Memory Trace, Raytracing, or Crash Analysis. |
| Connection | `RdpCaptureApiConnection` | A live link to an application through a graphics API (DX12, Vulkan, ...). Created automatically once an app connects. |
| App filter | `RdpCaptureAppFilter` | A callback that decides which process/API a context connects to. |
| Result code | `RdpCaptureResult` | Return value of most calls. `kRdpCaptureResultSuccess` (0) means success; negative values are errors. |

### The function table

`RdpCaptureGetFnTable()` is the **only** symbol exported from the shared
library. It fills in an `RdpCaptureFnTable` whose members you then call for every
subsequent operation. Pass the version macros so the implementation can verify
compatibility:

```c
RdpCaptureFnTable fn = {0};
RdpCaptureResult result = RdpCaptureGetFnTable(
    RDP_CAPTURE_API_VERSION_MAJOR,
    RDP_CAPTURE_API_VERSION_MINOR,
    RDP_CAPTURE_API_VERSION_PATCH,
    &fn);
if (result != kRdpCaptureResultSuccess)
{
    /* Version mismatch or load failure. */
}
```

---

## Linking and loading

There are two ways to obtain `RdpCaptureGetFnTable()`:

### 1. Link against the shared library (CMake)

The library ships a CMake package, so in-tree and installed consumers can use
`find_package`:

```cmake
find_package(RdpCaptureApi REQUIRED)
target_link_libraries(my_app PRIVATE RdpCaptureApi::RdpCaptureApi)
```

Then include the header and call the export directly:

```c
#include "RdpCaptureApi.h"
```

### 2. Load the DLL/shared object dynamically

If you prefer not to link at build time, load the library at runtime and resolve
the single export yourself:

```c
/* Windows */
HMODULE lib = LoadLibraryA("RdpCaptureApi.dll");
typedef RdpCaptureResult (*GetFnTablePfn)(uint32_t, uint32_t, uint32_t, RdpCaptureFnTable*);
GetFnTablePfn get_table = (GetFnTablePfn)GetProcAddress(lib, "RdpCaptureGetFnTable");
```

---

## Lifecycle at a glance

```
RdpCaptureGetFnTable()        // 1. get the function table
        |
   fn.initialize()            // 2. create the one-and-only context
        |
   fn.enable_feature()        // 3. enable features (before a process connects)
        |
   (application connects)     // 4. app filter accepts a process + API
        |
   fn.<feature>.begin_trace() // 5. start a capture
        |
   trace_finished callback    // 6. receive the captured data
        |
   fn.destroy()               // 7. tear down before exit
```

> **Important:** Only one context can exist per system. `initialize()` fails if
> the Radeon Developer Panel or Radeon Developer Service is already running, since
> they create a context too. Features can only be enabled or disabled while **no**
> process is connected.

---

## Quick start: capture a profile

The following sketch enables the Profiling feature, waits for a DirectX 12
application to connect, captures one profile, and writes the bytes delivered to
the trace-finished callback.

```c
#include "RdpCaptureApi.h"
#include <stdio.h>
#include <stdlib.h>

static void OnTraceFinished(void*                     user_data,
                            RdpCaptureFeature         feature,
                            RdpCaptureApiConnectionId connection,
                            RdpCaptureResult          result,
                            uint64_t                  size,
                            const uint8_t*            data)
{
    if (result != kRdpCaptureResultSuccess)
    {
        printf("Capture failed (%d)\n", result);
        return;
    }

    /* `data` is owned by the API and freed after this callback returns,
       so copy out anything you want to keep. */
    FILE* f = fopen("capture.rgp", "wb");
    if (f)
    {
        fwrite(data, 1, (size_t)size, f);
        fclose(f);
    }
}

/* Only connect to DirectX 12 applications. */
static uint8_t AppFilter(void* user_data, const RdpCaptureProcessInfo* info, RdpCaptureGpuApi api)
{
    return api == kRdpCaptureGpuApiDirectX12 ? 1 : 0;
}

int main(void)
{
    RdpCaptureFnTable fn = {0};
    if (RdpCaptureGetFnTable(RDP_CAPTURE_API_VERSION_MAJOR,
                             RDP_CAPTURE_API_VERSION_MINOR,
                             RDP_CAPTURE_API_VERSION_PATCH,
                             &fn) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    RdpCaptureContextInitParams init = {0};
    init.app_filter.filter = AppFilter;

    RdpCaptureContext context = NULL;
    if (fn.initialize(&init, &context) != kRdpCaptureResultSuccess)
    {
        return -1;
    }

    RdpCaptureFeatureEnableParams enable = {0};
    enable.feature                                = kRdpCaptureFeatureProfiling;
    enable.trace_finished_callback.trace_finished = OnTraceFinished;
    if (fn.enable_feature(context, &enable) != kRdpCaptureResultSuccess)
    {
        fn.destroy(context);
        return -1;
    }

    /* Wait until the connected app is ready to be traced. */
    while (fn.get_feature_stage(context, kRdpCaptureFeatureProfiling, 0)
           != kRdpCaptureFeatureStageCapturing)
    {
    }

    /* Capture from the first available connection. */
    fn.profiling.begin_trace(context, RDP_CAPTURE_API_FIRST_CONNECTION_ID);

    /* ... wait for OnTraceFinished (e.g. via your own flag) ... */

    fn.destroy(context);
    return 0;
}
```

---

## Features

Enable a feature with `fn.enable_feature()`, passing an
`RdpCaptureFeatureEnableParams` whose `feature` field selects which one and whose
`body` union carries the feature-specific options. Each feature also has its own
sub-table on `RdpCaptureFnTable` for per-feature operations.

| Feature | Enum | Sub-table | Produces |
|---|---|---|---|
| Profiling | `kRdpCaptureFeatureProfiling` | `fn.profiling` | RGP profile |
| Memory Trace | `kRdpCaptureFeatureMemoryTrace` | `fn.memory_trace` | RMV trace |
| Raytracing | `kRdpCaptureFeatureRaytracing` | `fn.raytracing` | Ray history trace |
| Crash Analysis | `kRdpCaptureFeatureCrashAnalysis` | `fn.crash_analysis` | `.rgd` crash dump |

- **Profiling** — Configure with `RdpCaptureProfilingParams` (SQTT buffer size,
  capture mode, frame terminators, instruction tracing). Call
  `fn.profiling.get_default_params()` for a sensible baseline, then
  `set_params()` and `begin_trace()`.
- **Memory Trace** — Use `fn.memory_trace.insert_snapshot()` to mark points of
  interest, then `dump_trace()` to write the RMV trace.
- **Raytracing** — Configure ray history buffer size and optional marker-based
  capture via `RdpCaptureRaytracingParams`.
- **Crash Analysis** — Enable with optional enhanced analysis and summary
  generation via `RdpCaptureCrashAnalysisEnableParams`; the trace arrives through
  the trace-finished callback as a `.rgd` dump.

### Other capabilities

The function table also exposes:

- **System info** — `fn.get_system_info()` returns OS/CPU/GPU details (mirrors
  the RDP System Info panel). Release it with `fn.free_system_info()`.
- **GPU clocks** — Query and set stable/peak clock modes with
  `fn.get_gpu_clock_modes()`, `fn.query_gpu_current_clock_mode()`, and
  `fn.set_gpu_current_clock_mode()`.
- **Driver experiments** — Discover and override driver experiments through
  `fn.experiments`.
- **Application blocklist** — Prevent specific processes from being captured
  using glob patterns via `fn.blocklist`.

---

## Memory ownership

The API allocates some return values on your behalf. Free them with the matching
function — never with the C runtime `free()`:

| Allocated by | Free with |
|---|---|
| `fn.get_api_connections()`, `fn.get_gpu_clock_modes()` | `fn.free()` |
| `fn.get_system_info()` | `fn.free_system_info()` |
| `fn.experiments.get_experiments()` | `fn.experiments.free_experiments()` |
| `fn.blocklist.get_entries()` | `fn.blocklist.free_entries()` |

The `data` buffer passed to the trace-finished callback is owned by the API and
is freed **after** the callback returns — copy out anything you need to keep.

---

## Callbacks and threading

Callbacks (`status_callback`, `progress_callback`, `log_callback`, the app
filter, and the trace-finished callback) may be invoked from internal API
threads. Implementations must:

- be thread-safe,
- return quickly, and
- **not** call back into the RDP Capture API from within a callback.

A given feature's status callback is never invoked concurrently with itself, but
callbacks for different features may overlap.

---

## Versioning

The API is versioned with the `RDP_CAPTURE_API_VERSION_MAJOR/MINOR/PATCH`
macros. Always pass these to `RdpCaptureGetFnTable()`; if the loaded library is
incompatible it returns `kRdpCaptureResultVersionMismatch` and the table is left
untouched.

## Result codes

Most calls return an `RdpCaptureResult`. Check against
`kRdpCaptureResultSuccess` (0); negative values indicate errors. See the
[`RdpCaptureResult`](@ref RdpCaptureResult) enum for the authoritative list.

| Code | Meaning |
|---|---|
| `kRdpCaptureResultSuccess` | The operation was successful. |
| `kRdpCaptureResultNotReady` | The operation was unavailable due to ready state. |
| `kRdpCaptureResultUnknown` | The operation's result was unknown. |
| `kRdpCaptureResultFailure` | The operation was not successful. |
| `kRdpCaptureResultUnsupported` | The operation was unsupported. |
| `kRdpCaptureResultNotFound` | Something required by the operation could not be found. |
| `kRdpCaptureResultAborted` | The operation was aborted. |
| `kRdpCaptureResultInvalidParams` | The parameters were invalid. |
| `kRdpCaptureResultVersionMismatch` | Version mismatch with the loaded library. |
