#!/bin/sh
# This script updates icons from the SVG file

BASEDIR=$(dirname "$0")

inkscape "$BASEDIR/blender_icons.svg" --export-dpi=90  --without-gui --export-png="$BASEDIR/blender_icons16.png"
inkscape "$BASEDIR/blender_icons.svg" --export-dpi=180 --without-gui --export-png="$BASEDIR/blender_icons32.png"

