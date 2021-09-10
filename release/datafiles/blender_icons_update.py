#!/usr/bin/env python3

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# This script updates icons from the SVG file
import os
import subprocess
import sys


def run(cmd, *, env=None):
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

inkscape_bin = os.environ.get("INKSCAPE_BIN", "inkscape")
blender_bin = os.environ.get("BLENDER_BIN", "blender")

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/MacOS/inkscape'
    if os.path.exists(inkscape_app_path):
        inkscape_bin = inkscape_app_path
    blender_app_path = '/Applications/Blender.app/Contents/MacOS/Blender'
    if os.path.exists(blender_app_path):
        blender_bin = blender_app_path

cmd = (
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
