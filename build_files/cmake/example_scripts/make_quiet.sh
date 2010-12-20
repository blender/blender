#!/bin/bash
# filters CMake output to be more like nan-makefiles

FILTER="^Scanning \|Linking \(C\|CXX\) static library \|Built target "
make $@ | grep --line-buffered -v "$FILTER" | sed  -e 's/^.*\//  /'
echo "Build Done"
