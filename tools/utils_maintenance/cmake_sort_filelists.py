#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Sorts CMake path lists
- Don't cross blank newline boundaries.
- Don't cross different path prefix boundaries.
"""
__all__ = (
    "main",
)

import os
import sys

PWD = os.path.dirname(__file__)
sys.path.append(os.path.join(PWD, "modules"))

from batch_edit_text import run

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(os.path.join(PWD, "..", ".."))))

# TODO, move to config file
SOURCE_DIRS = (
    "source",
    "intern/ghost",
)

SOURCE_EXT = (
    # C/C++
    ".c", ".h", ".cpp", ".hpp", ".cc", ".hh", ".cxx", ".hxx", ".inl",
    # Objective C
    ".m", ".mm",
)

# CMake commands where argument order may be significant (e.g. positional arguments),
# all lines within these commands are excluded from sorting.
SORT_SKIP_COMMANDS: set[str] = {
    "add_custom_command",
}

# CMake variable names where the order of entries may be significant,
# all lines within `set(<name> ...)` or `list(APPEND <name> ...)` are excluded from sorting.
SORT_SKIP_VARIABLES: set[str] = {
    "LIB",
    "TEST_LIB",
}


def sort_cmake_file_lists(fn: str, data_src: str) -> str | None:
    fn_dir = os.path.dirname(fn)
    lines = data_src.splitlines(keepends=True)

    def can_sort(l: str) -> bool:
        l = l.split("#", 1)[0].strip()
        # Source files.
        if l.endswith(SOURCE_EXT):
            if "(" not in l and ')' not in l:
                return True
        # Headers.
        if l and os.path.isdir(os.path.join(fn_dir, l)):
            return True
        # Libraries.
        if l.startswith(("bf_", "extern_")) and "." not in l and "/" not in l:
            return True
        return False

    def can_sort_compat(a: str, b: str) -> bool:
        # Strip comments.
        a = a.split("#", 1)[0]
        b = b.split("#", 1)[0]

        # Compare leading white-space.
        if a[:-(len(a.lstrip()))] == b[:-(len(b.lstrip()))]:
            # return False

            # Compare loading paths.
            a_ls = a.split("/")
            b_ls = b.split("/")
            if len(a_ls) == 1 and len(b_ls) == 1:
                return True
            if len(a_ls) == len(b_ls):
                if len(a_ls) == 1:
                    return True
                if a_ls[:-1] == b_ls[:-1]:
                    return True
        return False

    def calc_skip_lines(lines: list[str]) -> set[int]:
        """
        Compute line indices to skip sorting, where order may be significant
        (e.g. positional command arguments or library link order).
        """

        def is_skip_command(cmd_name: str, args: list[str]) -> bool:
            if cmd_name in SORT_SKIP_COMMANDS:
                return True
            if cmd_name == "set":
                if args and args[0] in SORT_SKIP_VARIABLES:
                    return True
            if cmd_name == "list":
                if len(args) >= 2 and args[1] in SORT_SKIP_VARIABLES:
                    return True
            return False

        skip_lines: set[int] = set()
        skip_depth = 0
        in_skip_cmd = False
        for i, line in enumerate(lines):
            line_strip = line.split("#", 1)[0]
            if not in_skip_cmd:
                ls = line_strip.lstrip()
                paren_pos = ls.find("(")
                if paren_pos != -1:
                    cmd_name = ls[:paren_pos].strip()
                    args = ls[paren_pos + 1:].split()
                    if is_skip_command(cmd_name, args):
                        in_skip_cmd = True
                        skip_depth = 0
            if in_skip_cmd:
                skip_lines.add(i)
                skip_depth += line_strip.count("(") - line_strip.count(")")
                if skip_depth <= 0:
                    in_skip_cmd = False
        return skip_lines

    skip_lines = calc_skip_lines(lines)

    i = 0
    while i < len(lines):
        if can_sort(lines[i]):
            j = i
            while j + 1 < len(lines):
                if not can_sort(lines[j + 1]):
                    break
                if not can_sort_compat(lines[i], lines[j + 1]):
                    break
                j = j + 1
            if i != j:
                # Skip blocks span full commands; their boundaries contain
                # parentheses which fail `can_sort`, so a sortable group
                # cannot straddle a skip boundary - checking `i` suffices.
                if i not in skip_lines:
                    lines[i:j + 1] = list(sorted(lines[i:j + 1]))
            i = j
        i = i + 1

    data_dst = "".join(lines)
    if data_src != data_dst:
        return data_dst
    return None


def main() -> int:
    run(
        directories=[os.path.join(SOURCE_DIR, d) for d in SOURCE_DIRS],
        is_text=lambda fn: fn.endswith("CMakeLists.txt"),
        text_operation=sort_cmake_file_lists,
        use_multiprocess=True,
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
