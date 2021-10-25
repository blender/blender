#!/bin/bash

# Use this script to generate splash.png from splash_2x.png.
# Supposed to give best quality image.
#
# Based on ImageMagic documentation, which is interesting
# to read anyway:
#
#  http://www.imagemagick.org/Usage/filter
#  http://www.imagemagick.org/Usage/filter/nicolas/

convert \
  splash_2x.png \
  -colorspace RGB \
  -filter Cosine \
  -resize 50% \
  -colorspace sRGB \
  splash.png
