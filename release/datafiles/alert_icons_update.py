#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import subprocess
import sys

BASEDIR = os.path.abspath(os.path.dirname(__file__))

inkscape_path = 'inkscape'

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/Resources/script'
    if os.path.exists(inkscape_app_path):
        inkscape_path = inkscape_app_path

cmd = (
    inkscape_path,
    os.path.join(BASEDIR, "alert_icons.svg"),
    "--export-width=1280",
    "--export-height=256",
    "--without-gui",
    "--export-png=" + os.path.join(BASEDIR, "alert_icons.png"),
)
subprocess.check_call(cmd)
