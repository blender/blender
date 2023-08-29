#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
This is just a wrapper to run Blender's QtCreator project file generator,
knowing only the CMake build path.

 qtc_project_update.py <project_path>
"""

import sys
import os

PROJECT_DIR = sys.argv[-1]


def cmake_find_source(path):
    import re
    match = re.compile(r"^CMAKE_HOME_DIRECTORY\b")
    cache = os.path.join(path, "CMakeCache.txt")
    with open(cache, 'r', encoding='utf-8') as f:
        for l in f:
            if re.match(match, l):
                return l[l.index("=") + 1:].strip()
    return ""


SOURCE_DIR = cmake_find_source(PROJECT_DIR)

cmd = (
    "python",
    os.path.join(SOURCE_DIR, "build_files/cmake/cmake_qtcreator_project.py"),
    "--build-dir",
    PROJECT_DIR,
)

print(cmd)
os.system(" ".join(cmd))
