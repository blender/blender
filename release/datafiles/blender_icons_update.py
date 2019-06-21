#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import subprocess
import sys

def run(cmd):
    print("   ", " ".join(cmd))
    subprocess.check_call(cmd)

BASEDIR = os.path.abspath(os.path.dirname(__file__))

inkscape_bin = os.environ.get("INKSCAPE_BIN", "inkscape")
blender_bin = os.environ.get("BLENDER_BIN", "blender")

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/Resources/script'
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
    "--without-gui",
    "--export-png=" + os.path.join(BASEDIR, "blender_icons16.png"),
)
run(cmd)

cmd = (
    inkscape_bin,
    os.path.join(BASEDIR, "blender_icons.svg"),
    "--export-width=1204",
    "--export-height=1280",
    "--without-gui",
    "--export-png=" + os.path.join(BASEDIR, "blender_icons32.png"),
)
run(cmd)


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
run(cmd)

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
run(cmd)

os.remove(os.path.join(BASEDIR, "blender_icons16.png"))
os.remove(os.path.join(BASEDIR, "blender_icons32.png"))

# For testing, if we want the PNG of each image
# ./datatoc_icon_split_to_png.py ./blender_icons16/*.dat
# ./datatoc_icon_split_to_png.py ./blender_icons32/*.dat
