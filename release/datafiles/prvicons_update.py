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

BASEDIR = os.path.abspath(os.path.dirname(__file__))

inkscape_bin = os.environ.get("INKSCAPE_BIN", "inkscape")

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/MacOS/inkscape'
    if os.path.exists(inkscape_app_path):
        inkscape_bin = inkscape_app_path

cmd = (
    inkscape_bin,
    os.path.join(BASEDIR, "prvicons.svg"),
    "--export-width=1792",
    "--export-height=256",
    "--export-type=png",
    "--export-filename=" + os.path.join(BASEDIR, "prvicons.png"),
)
subprocess.check_call(cmd)
