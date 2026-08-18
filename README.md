Radeon Developer Panel
----------------------
The Radeon Developer Panel (RDP) is a software tool that allows users to capture RGP profiles, RMV traces, RRA scenes, and RGD crash analysis dumps on Radeon GPUs.

The Radeon Developer Panel consists of 2 main components :

* The Radeon Developer Service (RDS) - A system tray application that
   unlocks the Developer Mode Driver features and supports
   communications with high level tools. A headless version is also
   available called RadeonDeveloperServiceCLI.
* The Radeon Developer Panel (RDP) - the tool that interacts with RDS.

## Building

Radeon Developer Panel uses CMake for its build automation.

Example usage for various build scenarios are listed here:

**All examples below assume they are run from a build directory located at repository root.**

For single configuration generators (**not** Visual Studio), we recommend creating config specific build directories such
as:

    mkdir cmake-build-debug
    cd cmake-build-debug

#### Ninja

The preferred generator to use is Ninja:

    cmake .. -GNinja -DCMAKE_PREFIX_PATH=<path_to_qt> -DCMAKE_BUILD_TYPE=Debug

#### Visual Studio

Generating a Visual Studio solution file can be achieved using the following:

    cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=<path_to_qt>


Documentation
-------------
The documentation for the Radeon Developer Panel can be found in each release. In the release .zip file or .tgz file, there will be a "docs" directory. Simply open the index.html file found under docs\help\rdp\html in a web browser to view the documentation. The documentation is also available from within the Radeon Developer Panel.

The documentation is hosted publicly at : http://radeon-developer-panel.readthedocs.io/en/latest/
