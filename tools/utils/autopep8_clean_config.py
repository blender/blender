# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "PATHS",
    "PATHS_EXCLUDE",
    "SOURCE_DIR",
)

import os

PATHS: tuple[str, ...] = (
    "build_files",
    "doc",
    "release/datafiles",
    "release/lts",
    "scripts/freestyle",
    "scripts/modules",
    "scripts/presets",
    "scripts/startup",
    "scripts/templates_py",
    "source/blender",
    "tools",
    "tests",
)

SOURCE_DIR = os.path.normpath(os.path.abspath(os.path.normpath(
    os.path.join(os.path.dirname(__file__), "..", ".."))))

PATHS = tuple(
    os.path.join(SOURCE_DIR, p.replace("/", os.sep))
    for p in PATHS
)

PATHS_EXCLUDE: set[str] = set(
    os.path.join(SOURCE_DIR, p.replace("/", os.sep))
    for p in
    (
        "tools/svn_rev_map/sha1_to_rev.py",
        "tools/svn_rev_map/rev_to_sha1.py",
        "tools/svn_rev_map/rev_to_sha1.py",
        "scripts/modules/_rna_manual_reference.py",
    )
)
