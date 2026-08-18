# Copyright Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

"""Generate the CLI blocklist defaults header from blocklist.ini.

Reads the QSettings-style blocklist.ini used by the GUI panel and emits a C++
header that defines kCliBlocklistEntries for the active platform. The CLI
#includes this header so its built-in blocklist stays in sync with the GUI's
defaults without checking the generated file into source control.
"""

import argparse
import os
import re
import sys

_ENTRY_RE = re.compile(r"^applications\\\d+\\name\s*=\s*(.+?)\s*$")

def parse_section(ini_path, section_name):
    entries = []
    in_section = False
    with open(ini_path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n").strip()
            if not line or line.startswith("#") or line.startswith(";"):
                continue
            if line.startswith("[") and line.endswith("]"):
                in_section = line[1:-1] == section_name
                continue
            if not in_section:
                continue
            m = _ENTRY_RE.match(line)
            if m:
                entries.append(m.group(1))
    return entries

def escape_cpp(s):
    return s.replace("\\", "\\\\").replace("\"", "\\\"")

def emit_array(out, name, entries):
    # std::array (not a C-style array) so size-0 cases are well-formed C++.
    out.write("constexpr std::array<const char*, {}> {} = {{".format(len(entries), name))
    if entries:
        out.write("\n")
        for e in entries:
            out.write("    \"{}\",\n".format(escape_cpp(e)))
    out.write("};\n")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("ini_file", help="Path to blocklist.ini")
    parser.add_argument("output", help="Path to generated header")
    args = parser.parse_args()

    win_entries = parse_section(args.ini_file, "Win32")
    linux_entries = parse_section(args.ini_file, "Linux")

    if not win_entries and not linux_entries:
        print("error: no blocklist entries parsed from {}".format(args.ini_file), file=sys.stderr)
        return 1

    with open(args.output, "w", encoding="utf-8") as out:
        # Emit only the basename so the generated header is byte-identical
        # across build directories and machines (good for build caches and
        # reproducible builds).
        out.write("// Auto-generated file. Do not edit.\n")
        out.write("// Generated from {}\n".format(os.path.basename(args.ini_file)))
        out.write("// See source/cli/blocklist_gen.py.\n\n")
        out.write("#pragma once\n\n")
        out.write("#include <array>\n\n")
        # Match the platform macros used in source/api/api_blocklist.cpp
        # (WIN32 rather than _WIN32) so the generated guards stay consistent
        # with the hand-written platform selection in the rest of the repo.
        out.write("#if defined(WIN32)\n")
        emit_array(out, "kCliBlocklistEntries", win_entries)
        out.write("#elif defined(__linux__)\n")
        emit_array(out, "kCliBlocklistEntries", linux_entries)
        out.write("#else\n")
        emit_array(out, "kCliBlocklistEntries", [])
        out.write("#endif\n")

    print("Generated {} ({} Win32, {} Linux)".format(
        args.output, len(win_entries), len(linux_entries)))
    return 0

if __name__ == "__main__":
    sys.exit(main())
