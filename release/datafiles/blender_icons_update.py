#!/usr/bin/env python3

# This script updates icons from the SVG file
import os

BASEDIR = os.path.abspath(os.path.dirname(__file__)) + os.sep

cmd = 'inkscape "%sblender_icons.svg" --export-dpi=90  --without-gui --export-png="%sblender_icons16.png"' % (BASEDIR, BASEDIR)
os.system(cmd)
cmd = 'inkscape "%sblender_icons.svg" --export-dpi=180 --without-gui --export-png="%sblender_icons32.png"' % (BASEDIR, BASEDIR)
os.system(cmd)

