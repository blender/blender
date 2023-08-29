# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
PATHS = (
    "build_files/build_environment/cmake",
    "build_files/cmake",
    "doc/python_api",
    "intern/clog",
    "intern/cycles",
    "intern/ghost",
    "intern/guardedalloc",
    "intern/memutil",
    "scripts/modules",
    "scripts",
    "source",
    "tests",

    # files
    "GNUmakefile",
    "make.bat",
)


SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", ".."))))

PATHS = tuple(
    os.path.join(SOURCE_DIR, p.replace("/", os.sep))
    for p in PATHS
)


def files(path, test_fn):
    for dirpath, dirnames, filenames in os.walk(path):
        # skip '.git'
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for filename in filenames:
            if test_fn(filename):
                filepath = os.path.join(dirpath, filename)
                yield filepath


PATHS = PATHS + tuple(
    files(
        os.path.join(SOURCE_DIR),
        lambda filename: filename in {"CMakeLists.txt"} or filename.endswith((".cmake"))
    )
)
