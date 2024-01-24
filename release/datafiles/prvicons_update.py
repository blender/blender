#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2014-2022 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# This script updates icons from the SVG file
import os
import subprocess
import sys

BASEDIR = os.path.abspath(os.path.dirname(__file__))

if sys.platform == 'darwin':
    inkscape_bin = '/Applications/Inkscape.app/Contents/MacOS/inkscape'
else:
    inkscape_bin = "inkscape"
inkscape_bin = os.environ.get("INKSCAPE_BIN", inkscape_bin)

cmd = (
    inkscape_bin,
    os.path.join(BASEDIR, "prvicons.svg"),
    "--export-width=1792",
    "--export-height=256",
    "--export-type=png",
    "--export-filename=" + os.path.join(BASEDIR, "prvicons.png"),
)
subprocess.check_call(cmd)
