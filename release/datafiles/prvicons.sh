#!/bin/sh
# This script updates icons from the SVG file

BASEDIR=$(dirname "$0")

inkscape "$BASEDIR/prvicons.svg" --without-gui --export-png="$BASEDIR/prvicons.png"

