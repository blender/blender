#!/usr/bin/env python3

# This script updates icons from the SVG file
import os
import sys

BASEDIR = os.path.abspath(os.path.dirname(__file__)) + os.sep

inkscape_path = 'inkscape'

if sys.platform == 'darwin':
    inkscape_app_path = '/Applications/Inkscape.app/Contents/Resources/script'
    if os.path.exists(inkscape_app_path):
        inkscape_path = inkscape_app_path

cmd = inkscape_path + ' "%sprvicons.svg" --without-gui --export-png="%sprvicons.png"' % (BASEDIR, BASEDIR)
os.system(cmd)
