#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import subprocess
import sys

BASEDIR = os.path.abspath(os.path.dirname(__file__))

inkscape_path = 'inkscape'

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/MacOS/inkscape'
    if os.path.exists(inkscape_app_path):
        inkscape_path = inkscape_app_path

cmd = (
    inkscape_path,
    os.path.join(BASEDIR, "prvicons.svg"),
    "--export-width=1792",
    "--export-height=256",
    "--export-type=png",
    "--export-filename=" + os.path.join(BASEDIR, "prvicons.png"),
)
subprocess.check_call(cmd)
