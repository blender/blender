#!/bin/sh
# This script updates icons from the SVG file

inkscape blender_icons.svg --export-dpi=90  --without-gui --export-png=blender_icons16.png
inkscape blender_icons.svg --export-dpi=180 --without-gui --export-png=blender_icons32.png
