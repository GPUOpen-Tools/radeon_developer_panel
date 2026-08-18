Radeon Developer Panel CLI
==========================

``RadeonDeveloperPanelCLI(.exe)`` is a headless, command-line version of the
Radeon Developer Panel. It is designed for automated pipelines, remote GPU
systems, and scripted capture workflows where no GUI is available.

Usage
-----

.. code-block:: text

    RadeonDeveloperPanelCLI [OPTION...] [MODE OPTIONS...]

General options
---------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Option
     - Description
   * - ``-o, --output <path>``
     - Output file path. Defaults to ``<mode>_<timestamp>.<ext>``.
   * - ``-m, --mode <mode>``
     - Capture mode: ``profiling``, ``raytracing``, ``memory``, ``crash``, ``clocks``. Default: ``profiling``.
   * - ``-p, --process <name>``
     - Filter: only connect to processes whose name contains this string.
   * - ``--remote-host <host>``
     - Remote hostname or IP address to connect to.
   * - ``--remote-port <port>``
     - Remote port. Default: 27300.
   * - ``--verbose``
     - Enable verbose logging.
   * - ``-h, --help [=<mode>]``
     - Print usage. Use ``--help=<mode>`` for mode-specific options.
   * - ``-v, --version``
     - Print version information.

Profiling (RGP) options
-----------------------

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Option
     - Description
   * - ``-a, --rgp-auto-capture <spec>``
     - Auto-capture mode. Formats: ``frame[:N]`` captures at frame N (default 0); ``dispatch[:start[:count]]`` captures a range of dispatches.
   * - ``--rgp-auto-capture-delay <ms>``
     - Delay in milliseconds before dispatch auto-capture starts. Default: 0.
   * - ``--rgp-capture-mode <mode>``
     - RGP capture mode: ``default``, ``frame``, ``draw``, ``dispatch``. ``default`` lets the driver choose based on the API. Default: ``default``.
   * - ``--rgp-render-op-count <count>``
     - Number of render operations to capture in ``draw`` or ``dispatch`` mode (ignored in ``frame`` mode). Default: 1.
   * - ``--rgp-instruction-tracing``
     - Enable instruction-level tracing for detailed shader analysis.
   * - ``--rgp-counter-collection``
     - Enable hardware counter collection.
   * - ``--rgp-shader-instrumentation``
     - Enable shader instrumentation.
   * - ``--rgp-sqtt-buffer-size <size>``
     - SQTT buffer size: ``minimum``, ``low``, ``default``, ``high``, ``maximum``. Default: ``default``.

Raytracing (RRA) options
------------------------

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Option
     - Description
   * - ``--rra-ray-history-buffer-size <size>``
     - Ray history buffer size: ``disabled``, ``minimum``, ``low``, ``default``, ``high``, ``maximum``. Default: ``default``. This option requires ray dispatch data collection to be enabled; explicit arguments are rejected when ``--rra-collect-ray-dispatch-data=false``.
   * - ``--rra-collect-ray-dispatch-data <true|false>``
     - Collect ray dispatch data (ray history). Pass ``--rra-collect-ray-dispatch-data=false`` to disable ray history collection. When disabled, explicit ``--rra-ray-history-buffer-size`` arguments are rejected by the CLI. Default: ``true``.
   * - ``--rra-delay-ms <ms>``
     - Delay in milliseconds before triggering each raytracing capture (0 = no delay). Default: 0.

Crash Analysis (RGD) options
-----------------------------

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Option
     - Description
   * - ``--rgd-enhanced``
     - Enable enhanced crash analysis. May affect application performance.
   * - ``--rgd-text-summary``
     - Request a text crash summary alongside the ``.rgd`` (consumed by downstream tooling such as the RDP UI or ``rgd.exe``).
   * - ``--rgd-json-summary``
     - Request a JSON crash summary alongside the ``.rgd`` (consumed by downstream tooling such as the RDP UI or ``rgd.exe``).
   * - ``--rgd-marker-source``
     - Display execution-marker source information in the crash summary.
   * - ``--rgd-expand-markers``
     - Expand all execution-marker nodes in the crash summary.
   * - ``--rgd-pdb-search-path <path>``
     - DXC shader PDB search path. Can be specified multiple times.
   * - ``--rgd-pdb-include-subfolders``
     - Recurse into subfolders when resolving DXC shader PDBs.
   * - ``--rgd-cli-path <path>``
     - Path to the ``rgd`` executable used to generate text / JSON summaries (file or directory). Defaults to the current working directory.
   * - ``--rgd-collect-sgprs``
     - Collect wave SGPRs during enhanced crash analysis. Requires ``--rgd-enhanced``.
   * - ``--rgd-collect-vgprs``
     - Collect wave VGPRs during enhanced crash analysis. Requires ``--rgd-enhanced``.

Clocks options
--------------

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Option
     - Description
   * - ``--clock-mode <mode>``
     - Clock mode to set: ``normal`` or ``stable``.
   * - ``--gpu-index <index>``
     - GPU index to target. Default: 0.

Driver Experiments options
--------------------------

Driver experiments allow overriding individual driver settings at capture time.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Option
     - Description
   * - ``--driver-experiment <spec>``
     - Set a driver experiment override. Can be specified multiple times. Formats: ``api/name=value`` (e.g. ``vulkan/ShaderOpt=true``), ``0xID=value``, or ``name=value``.
   * - ``--list-experiments``
     - List all available driver experiments and exit.

Example — list experiments, then enable one:

.. code-block:: bash

    # List all available experiments
    RadeonDeveloperPanelCLI --list-experiments

    # Apply an experiment during capture
    RadeonDeveloperPanelCLI --driver-experiment "vulkan/ShaderOpt=true"

.. _CliSystemInfo:

System info option
------------------

Print details about the connected system and exit. The output mirrors the
information displayed in the RDP UI's System Info panel and includes:

* Operating system: name, description, hostname, physical memory type/size,
  swap memory size.
* Driver: name, description, packaging version, packaging date, software
  version.
* CPU(s): name, architecture, vendor ID, CPU ID, device ID, physical/logical
  core counts, max clock speed and virtualization state (when available).
* GPU(s): name, shader engine clock range, timestamp frequency, ASIC family/
  device/revision/eRev, memory (type, bandwidth, bus width, clock range,
  operations per clock, heaps), PCI bus/device/function/packed ID, and the
  Big Software major/minor/misc version.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Option
     - Description
   * - ``--system-info``
     - Print system info (OS, driver, CPU and GPU details) and exit.

When combined with other one-shot info flags such as ``--list-experiments`` or
``--list-blocklist``, all requested info is printed before the CLI exits (the
order is fixed: blocklist, system info, experiments).

.. code-block:: bash

    # Print system info only
    RadeonDeveloperPanelCLI --system-info

    # Print system info and the list of available experiments together
    RadeonDeveloperPanelCLI --system-info --list-experiments

.. _CliBlocklist:

Blocklist options
-----------------

The blocklist prevents specific processes from connecting to the panel during a
capture session. This mirrors the **Blocked applications** functionality
available in the GUI panel (see :ref:`BlockedApplications`).

At startup, the CLI applies a built-in set of default blocklist entries that is
compiled into the executable. To load additional blocklist entries from a file
at runtime, use ``--block-file``. Individual entries can also be supplied
directly via ``--block``.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - Option
     - Description
   * - ``--block <pattern>``
     - Block a process name pattern from connecting. Can be specified multiple times.
   * - ``--block-file <path>``
     - Load additional blocklist entries from a text file (one pattern per line; lines starting with ``#`` are comments).
   * - ``--list-blocklist``
     - Print all active blocklist entries and exit.

**Wildcard matching**

The same wildcard syntax used by the GUI panel is supported:

* ``*`` — matches zero or more of any character
* ``?`` — matches exactly one character
* ``[...]`` — matches one character from the set (e.g. ``[abc]``)
* ``[!...]`` — matches one character NOT in the set (e.g. ``[!0-9]``)
* ``[a-z]`` — character ranges within a set

Use a backslash to escape any of these special characters.

Examples:

.. code-block:: text

    [Gg]ears.exe      # gears.exe with upper or lower-case G
    gpu_info*         # any process whose name starts with gpu_info
    test?.exe         # test1.exe, test6.exe, etc.

**--block-file format**

A file passed to ``--block-file`` lists one pattern per line. Empty lines and
lines beginning with ``#`` are ignored:

.. code-block:: text

    # Launchers to skip
    EpicGamesLauncher.exe
    GOGGalaxy.exe
    steam*

**Workflow examples**

*Inspect the built-in defaults:*

.. code-block:: bash

    RadeonDeveloperPanelCLI --list-blocklist

*Block a single additional process and start a profiling capture:*

.. code-block:: bash

    RadeonDeveloperPanelCLI --block MyLauncher.exe

*Block all processes matching a pattern:*

.. code-block:: bash

    RadeonDeveloperPanelCLI --block "steam*"

*Load extra entries from a file and verify the combined list:*

.. code-block:: bash

    # my_extra_blocks.txt:
    #   # Launchers to skip
    #   EpicGamesLauncher.exe
    #   GOGGalaxy.exe

    RadeonDeveloperPanelCLI --block-file my_extra_blocks.txt --list-blocklist

*Both --block and --block-file together:*

.. code-block:: bash

    RadeonDeveloperPanelCLI \
        --block "steam*" \
        --block-file my_extra_blocks.txt

Both sources are merged with the CLI's built-in default entries.

**Console feedback**

When a process is blocked, the CLI prints a message to standard output:

.. code-block:: text

    Blocked process: MyLauncher.exe (PID: 12345)

This lets you confirm that the blocklist is working as expected without
attaching a debugger.
