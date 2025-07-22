#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2018-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This script updates icons from the BLEND file
__all__ = (
    "main",
)

import os
import subprocess
import sys

from collections.abc import (
    Iterator,
    Sequence,
)

BASEDIR = os.path.abspath(os.path.dirname(__file__))
ROOTDIR = os.path.normpath(os.path.join(BASEDIR, "..", ".."))


def run(cmd: Sequence[str], *, env: dict[str, str] | None = None) -> None:
    print("   ", " ".join(cmd))
    subprocess.check_call(cmd, env=env)


def edit_text_file(filename: str, marker_begin: str, marker_end: str, content: str) -> None:
    with open(filename, 'r', encoding='utf-8') as f:
        data = f.read()
    marker_begin_index = data.find(marker_begin)
    marker_end_index = data.find(marker_end, marker_begin_index)
    # include indentation of marker
    while data[marker_end_index - 1] in {'\t', ' '}:
        marker_end_index -= 1
    if marker_begin_index == -1:
        print('Error: {!r} not found'.format(marker_begin))
        return
    if marker_end_index == -1:
        print('Error: {!r} not found'.format(marker_end))
        return
    marker_begin_index += len(marker_begin) + 1
    data_update = data[:marker_begin_index] + content + data[marker_end_index:]
    if data != data_update:
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(data_update)


def main() -> int:
    blender_bin = os.environ.get("BLENDER_BIN", "blender")
    if not os.path.exists(blender_bin):
        blender_bin = os.path.join(ROOTDIR, "blender.bin")

    if not os.path.exists(blender_bin):
        if sys.platform == 'darwin':
            blender_app_path = '/Applications/Blender.app/Contents/MacOS/Blender'
            if os.path.exists(blender_app_path):
                blender_bin = blender_app_path

    icons_blend = (
        os.path.join(ROOTDIR, "release", "datafiles", "icons_blend", "toolbar.blend"),
    )

    def names_and_time_from_path(path: str) -> Iterator[tuple[str, float]]:
        for entry in os.scandir(path):
            name = entry.name
            if name.endswith(".dat"):
                yield (name, entry.stat().st_mtime)

    # Collect icons files and update CMake.
    icon_files = []

    # Create `.dat` geometry (which are stored in GIT).
    for blend in icons_blend:
        output_dir = os.path.join(BASEDIR, "icons")
        files_old = set(names_and_time_from_path(output_dir))
        cmd = (
            blender_bin, "--background", "--factory-startup",
            blend,
            "--python", os.path.join(BASEDIR, "blender_icons_geom.py"),
            "--",
            "--group", "Export",
            "--output-dir", output_dir,
        )

        env = {}
        # Developers may have ASAN enabled, avoid non-zero exit codes.
        env["ASAN_OPTIONS"] = "exitcode=0:" + os.environ.get("ASAN_OPTIONS", "")
        # These NEED to be set on windows for python to initialize properly.
        if sys.platform[:3] == "win":
            env["PATHEXT"] = os.environ.get("PATHEXT", "")
            env["SystemDrive"] = os.environ.get("SystemDrive", "")
            env["SystemRoot"] = os.environ.get("SystemRoot", "")

        run(cmd, env=env)
        files_new = set(names_and_time_from_path(output_dir))

        icon_files.extend([
            name[:-4]  # No `.dat`.
            for (name, _) in sorted((files_new - files_old))
        ])

    edit_text_file(
        os.path.join(ROOTDIR, "source", "blender", "editors", "datafiles", "CMakeLists.txt"),
        "# BEGIN ICON_GEOM_NAMES",
        "# END ICON_GEOM_NAMES",
        "  " + "\n  ".join(icon_files) + "\n",
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
