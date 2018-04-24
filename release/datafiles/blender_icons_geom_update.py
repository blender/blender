#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import subprocess
import sys

def run(cmd):
    print("   ", " ".join(cmd))
    subprocess.check_call(cmd)

BASEDIR = os.path.abspath(os.path.dirname(__file__))
ROOTDIR = os.path.normpath(os.path.join(BASEDIR, "..", ".."))

blender_bin = os.environ.get("BLENDER_BIN", "blender")
if not os.path.exists(blender_bin):
    blender_bin = os.path.join(ROOTDIR, "blender.bin")

icons_blend = (
    os.path.join(ROOTDIR, "..", "lib", "resources", "icon_geom.blend"),
)

# create .dat pixmaps (which are stored in git)
for blend in icons_blend:
    cmd = (
        blender_bin, "--background", "--factory-startup", "-noaudio",
        blend,
        "--python", os.path.join(BASEDIR, "blender_icons_geom.py"),
        "--",
        "--output-dir", os.path.join(BASEDIR, "icons"),
    )
    run(cmd)
