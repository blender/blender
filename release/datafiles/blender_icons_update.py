#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import sys

def run(cmd):
    print("   ", cmd)
    os.system(cmd)

BASEDIR = os.path.abspath(os.path.dirname(__file__)) + os.sep

inkscape_path = 'inkscape'

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/Resources/script'
    if os.path.exists(inkscape_app_path):
        inkscape_path = inkscape_app_path

cmd = inkscape_path + ' "%sblender_icons.svg" --export-dpi=90  --without-gui --export-png="%sblender_icons16.png"' % (BASEDIR, BASEDIR)
run(cmd)
cmd = inkscape_path + ' "%sblender_icons.svg" --export-dpi=180 --without-gui --export-png="%sblender_icons32.png"' % (BASEDIR, BASEDIR)
run(cmd)


# For testing it can be good to clear all old
# rm ./blender_icons16/*.dat
# rm ./blender_icons32/*.dat

datatoc_icon_split_py = os.path.join(BASEDIR, "..", "..", "source", "blender", "datatoc", "datatoc_icon_split.py")

# create .dat pixmaps (which are stored in git)
cmd = (
    "blender "
    "--background -noaudio "
    "--python " + datatoc_icon_split_py + " -- "
    "--image=" + BASEDIR + "blender_icons16.png "
    "--output=" + BASEDIR + "blender_icons16 "
    "--output_prefix=icon16_ "
    "--name_style=UI_ICONS "
    "--parts_x 26 --parts_y 30 "
    "--minx 3 --maxx 53 --miny 3 --maxy 8 "
    "--minx_icon 2 --maxx_icon 2 --miny_icon 2 --maxy_icon 2 "
    "--spacex_icon 1 --spacey_icon 1"
    )
run(cmd)

cmd = (
    "blender "
    "--background -noaudio "
    "--python " + datatoc_icon_split_py + " -- "
    "--image=" + BASEDIR + "blender_icons32.png "
    "--output=" + BASEDIR + "blender_icons32 "
    "--output_prefix=icon32_ "
    "--name_style=UI_ICONS "
    "--parts_x 26 --parts_y 30 "
    "--minx 6 --maxx 106 --miny 6 --maxy 16 "
    "--minx_icon 4 --maxx_icon 4 --miny_icon 4 --maxy_icon 4 "
    "--spacex_icon 2 --spacey_icon 2"

    )
run(cmd)

os.remove(BASEDIR + "blender_icons16.png")
os.remove(BASEDIR + "blender_icons32.png")

# For testing, if we want the PNG of each image
# ./datatoc_icon_split_to_png.py ./blender_icons16/*.dat
# ./datatoc_icon_split_to_png.py ./blender_icons32/*.dat

