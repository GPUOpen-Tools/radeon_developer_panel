Radeon Developer Panel
==========================

Initial setup
=============

**IMPORTANT:** 
      The application you want to trace must **NOT** already be
      running. The panel needs to be configured in advance of starting your
      application.

1) Start the **RadeonDeveloperPanel.exe** on your local system. The
   panel will startup up with the Connection tab already highlighted
   (see below).

.. image:: media/Connection_1.png
..

   The connection panel has three main elements:

-  **Connection status** – to the Radeon Developer Service (currently
   not connected)

-  **Connection dropdown** - choose a previous connection to connect to. **Local** will always
   be available in this list

-  **New connection** – section that allows you specify a new remote connection. New connections
   will be added to the connections list.

2) Connect the **Radeon Developer Panel** to the **Radeon Developer
   Service**:

      **Select an entry from the Connection dropdown,
      then click the “Connect” button.** This will start the Radeon
      Developer Service on the local system and establish a connection.

Note that the red indicator to the left of the “CONNECTION” tab will change to
green to indicate that the connection was successful.

Remote connections
==================

1) Start the **RadeonDeveloperService.exe** on the **remote** system (the machine
where the application is to be run). Make a note of the remote system's IP address
(open a command prompt and type 'ipconfig').

2) Start the **RadeonDeveloperPanel.exe** on the local system. On the **CONNECTION**
tab, enter the IP address of the **remote** system in the **Host name** and then
click the  “Connect” button.

Application list
================

After a connection is made to the service, the panel will switch to the
**System** tab.

.. image:: media/System_1.png
..

   The system tab contains various panels for configuration:

- **My applications** - List of applications enabled for driver connection

- **Application blacklist** - List of applications blacklisted from driver connection

In the **My applications** list, using the edit line
or by selecting from the filesystem add an application
to the list, specifying an **API** type
from the combobox to check for when running the application.

**IMPORTANT**
      The **API** specified works as a filter against the client application
      accepting the driver connection. If you are unsure of what **API** is being used
      or don't care use the default **Auto**

Once an application is added to the list, it can then be run on the system to
start a driver connection. 

When a connection to the client application has
been established, the panel will then switch to the **Applications** tab.

**IMPORTANT**
      When the **Memory Trace** workflow is enabled for an application entry,
      memory tracing will be started when the application is launched.


How to memory trace your application
====================================

Upon running an application successfully the panel will have switched
to the **Applications** tab shown here:

.. image:: media/MemoryTrace_01.png

**NOTE**
   Memory tracing will have been implicitly started when the application was launched.

The memory trace UI has the following elements:

-  **Dump trace** – stops memory tracing and writes results to disk

-  **Insert snapshot** - insert user specified identifier to define snapshot in trace. A
   snapshot captures a moment in time in much the same way as a photograph. For example, to
   spot memory leaks, 2 snapshots can be added; one just before a game level is started after
   the menu screens and another snapshot when the game level finishes once the user is back in
   the game menus. Theoretically, the game should be in the same state in both cases (in the menus
   before and after a game level).

-  **Recently collected traces** – displays any recently collected traces in output directory

-  **Trace settings** - specify trace output directory and **Radeon Memory Visualizer** path

Writing out the memory trace to file can be achieved by one of the following:

* **Close the running application**

   When the client application terminates, the memory tracing
   will stop and the results will be written to disk.

* **Click the Dump trace button**

   Clicking the **Dump trace** button from the Memory Trace UI will stop
   memory tracing and write the results to disk.

Using either of the above methods to complete memory tracing
will result in a **Radeon Memory Visualizer** trace file being written to disk.

Example output:

   sample_20200316-143712.rmv

**IMPORTANT:**
      Once a memory trace has finished either through closing the application or
      through clicking the **Dump trace** button. The application **MUST** be
      closed and re-launched to start a new memory trace.