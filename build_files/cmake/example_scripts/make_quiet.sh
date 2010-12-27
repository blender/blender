#!/bin/bash
# filters CMake output to be more like nan-makefiles

FILTER="^\[ *[0-9]*%] \|^Built target \|^Scanning "
make $@ | \
		sed -u -e 's/^Linking .*\//Linking /' | \
		sed -u -e 's/^.*\//  /' | \
		grep --line-buffered -v "$FILTER"

echo "Build Done"
