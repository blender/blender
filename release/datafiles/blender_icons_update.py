#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2014-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This script updates icons from the SVG file
import os
import subprocess
import sys

from typing import (
    Dict,
    List,
    Optional,
    Sequence,
    Tuple,
)


def run(cmd: Sequence[str], *, env: Optional[Dict[str, str]] = None) -> None:
    print("   ", " ".join(cmd))
    subprocess.check_call(cmd, env=env)


BASEDIR = os.path.abspath(os.path.dirname(__file__))

env = {}
# Developers may have ASAN enabled, avoid non-zero exit codes.
env["ASAN_OPTIONS"] = "exitcode=0:" + os.environ.get("ASAN_OPTIONS", "")

# These NEED to be set on windows for python to initialize properly.
if sys.platform[:3] == "win":
    env["PATHEXT"] = os.environ.get("PATHEXT", "")
    env["SystemDrive"] = os.environ.get("SystemDrive", "")
    env["SystemRoot"] = os.environ.get("SystemRoot", "")

if not (inkscape_bin := os.environ.get("INKSCAPE_BIN")):
    if sys.platform == 'darwin':
        inkscape_bin = '/Applications/Inkscape.app/Contents/MacOS/inkscape'
    else:
        inkscape_bin = "inkscape"

blender_bin = os.environ.get("BLENDER_BIN", "blender")

cmd: Tuple[str, ...] = (
    inkscape_bin,
    os.path.join(BASEDIR, "blender_icons.svg"),
    "--export-width=602",
    "--export-height=640",
    "--export-type=png",
    "--export-filename=" + os.path.join(BASEDIR, "blender_icons16.png"),
)
run(cmd, env=env)

cmd = (
    inkscape_bin,
    os.path.join(BASEDIR, "blender_icons.svg"),
    "--export-width=1204",
    "--export-height=1280",
    "--export-type=png",
    "--export-filename=" + os.path.join(BASEDIR, "blender_icons32.png"),
)
run(cmd, env=env)


# For testing it can be good to clear all old
# rm ./blender_icons16/*.dat
# rm ./blender_icons32/*.dat

datatoc_icon_split_py = os.path.join(BASEDIR, "..", "..", "source", "blender", "datatoc", "datatoc_icon_split.py")

# create .dat pixmaps (which are stored in git)
cmd = (
    blender_bin, "--background", "--factory-startup", "-noaudio",
    "--python", datatoc_icon_split_py, "--",
    "--image=" + os.path.join(BASEDIR, "blender_icons16.png"),
    "--output=" + os.path.join(BASEDIR, "blender_icons16"),
    "--output_prefix=icon16_",
    "--name_style=UI_ICONS",
    "--parts_x", "26", "--parts_y", "30",
    "--minx", "3", "--maxx", "53", "--miny", "3", "--maxy", "8",
    "--minx_icon", "2", "--maxx_icon", "2", "--miny_icon", "2", "--maxy_icon", "2",
    "--spacex_icon", "1", "--spacey_icon", "1",
)
run(cmd, env=env)

cmd = (
    blender_bin, "--background", "--factory-startup", "-noaudio",
    "--python", datatoc_icon_split_py, "--",
    "--image=" + os.path.join(BASEDIR, "blender_icons32.png"),
    "--output=" + os.path.join(BASEDIR, "blender_icons32"),
    "--output_prefix=icon32_",
    "--name_style=UI_ICONS",
    "--parts_x", "26", "--parts_y", "30",
    "--minx", "6", "--maxx", "106", "--miny", "6", "--maxy", "16",
    "--minx_icon", "4", "--maxx_icon", "4", "--miny_icon", "4", "--maxy_icon", "4",
    "--spacex_icon", "2", "--spacey_icon", "2",
)
run(cmd, env=env)

os.remove(os.path.join(BASEDIR, "blender_icons16.png"))
os.remove(os.path.join(BASEDIR, "blender_icons32.png"))

# For testing, if we want the PNG of each image
# ./datatoc_icon_split_to_png.py ./blender_icons16/*.dat
# ./datatoc_icon_split_to_png.py ./blender_icons32/*.dat
