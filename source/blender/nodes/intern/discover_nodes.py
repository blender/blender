# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

'''
Usage:
    python discover_nodes.py
        <sources/root>
        <path/to/output.cc>
        <generated_function_name>
        <source>...

The goal is to make it easy for nodes to register themselves without having to have
a central place that registers all nodes manually. A node can use this mechanism by
invoking `NOD_REGISTER_NODE(register_function_name)`.

This scripts finds all those macro invocations generates code that calls the functions.
'''

import re
import sys
from pathlib import Path

source_root = Path(sys.argv[1])
output_cc_file = Path(sys.argv[2])
function_to_generate = sys.argv[3]
relative_source_files = sys.argv[4:]

macro_name = "NOD_REGISTER_NODE"
discover_suffix = "_discover"

include_lines = []
decl_lines = []
func_lines = []

# Add forward declaration to avoid warning.
func_lines.append(f"void {function_to_generate}();")
func_lines.append(f"void {function_to_generate}()")
func_lines.append("{")

# Use a single regular expression to search for opening namespaces, closing namespaces
# and macro invocations. This makes it easy to iterate over the matches in order.
re_namespace_begin = r"^namespace ([\w:]+) \{"
re_namespace_end = r"^\}  // namespace ([\w:]+)"
re_macro = r"MACRO\((\w+)\)".replace("MACRO", macro_name)
re_all = f"({re_namespace_begin})|({re_namespace_end})|({re_macro})"

for relative_source_file in relative_source_files:
    if not relative_source_file.endswith(".cc"):
        continue
    path = source_root / relative_source_file

    # Read the source code.
    with open(path) as f:
        code = f.read()

    # Keeps track of the current namespace we're in.
    namespace_parts = []

    for match in re.finditer(re_all, code, flags=re.MULTILINE):
        if entered_namespace := match.group(2):
            # Enter a (nested) namespace.
            namespace_parts += entered_namespace.split("::")
        elif exited_namespace := match.group(4):
            # Exit a (nested) namespace.
            del namespace_parts[-len(exited_namespace.split("::")):]
        elif function_name := match.group(6):
            # Macro invocation in the current namespace.
            namespace_str = "::".join(namespace_parts)
            # Add suffix so that this refers to the function created by the macro.
            auto_run_name = function_name + discover_suffix

            # Declare either outside of any named namespace or in a (nested) namespace.
            # Can't declare it in an anonymous namespace because that would make the
            # declared function static.
            if namespace_str:
                decl_lines.append(f"namespace {namespace_str} {{")
            decl_lines.append(f"void {auto_run_name}();")
            if namespace_str:
                decl_lines.append(f"}}")

            # Call the function.
            func_lines.append(f"  {namespace_str}::{auto_run_name}();")

func_lines.append("}")

# Write the generated code if it changed. If the newly generated code is the same as before,
# don't overwrite the existing file to avoid unnecessary rebuilds.
try:
    with open(output_cc_file) as f:
        old_generated_code = f.read()
except:
    old_generated_code = ""
new_generated_code = "\n".join(include_lines + decl_lines + [""] + func_lines)

if old_generated_code != new_generated_code:
    with open(output_cc_file, "w") as f:
        f.write(new_generated_code)
