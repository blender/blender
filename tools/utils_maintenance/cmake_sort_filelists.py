#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Sorts CMake path lists
- Don't cross blank newline boundaries.
- Don't cross different path prefix boundaries.
"""

import os
import sys

from typing import (
    Optional,
)

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


def sort_cmake_file_lists(fn: str, data_src: str) -> Optional[str]:
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
                lines[i:j + 1] = list(sorted(lines[i:j + 1]))
            i = j
        i = i + 1

    data_dst = "".join(lines)
    if data_src != data_dst:
        return data_dst
    return None


run(
    directories=[os.path.join(SOURCE_DIR, d) for d in SOURCE_DIRS],
    is_text=lambda fn: fn.endswith("CMakeLists.txt"),
    text_operation=sort_cmake_file_lists,
    use_multiprocess=True,
)
