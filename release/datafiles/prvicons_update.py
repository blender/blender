#!/usr/bin/env python3

# This script updates icons from the SVG file
import os

BASEDIR = os.path.abspath(os.path.dirname(__file__)) + os.sep

cmd = 'inkscape "%sprvicons.svg" --without-gui --export-png="%sprvicons.png"' % (BASEDIR, BASEDIR)
os.system(cmd)

