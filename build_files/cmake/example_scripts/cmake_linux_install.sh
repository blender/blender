#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

# This shell script checks out and compiles blender, tested on ubuntu 10.04
# assumes you have dependencies installed already

# See this page for more info:
#   https://wiki.blender.org/wiki/Building_Blender/Linux/Generic_Distro/CMake

# grab blender
mkdir ~/blender-git
cd ~/blender-git

git clone https://projects.blender.org/blender/blender.git
cd blender
git submodule update --init --recursive
git submodule foreach git checkout main
git submodule foreach git pull --rebase origin main

# create build dir
mkdir ~/blender-git/build-cmake
cd ~/blender-git/build-cmake

# cmake without copying files for fast rebuilds
# the files from git will be used in place
cmake ../blender

# make blender, will take some time
make -j$(nproc)

# link the binary to blenders source directory to run quickly
ln -s ~/blender-git/build-cmake/bin/blender ~/blender-git/blender/blender.bin

# useful info
echo ""
echo "* Useful Commands *"
echo "   Run Blender: ~/blender-git/blender/blender.bin"
echo "   Update Blender: git pull --rebase; git submodule foreach git pull --rebase origin main"
echo "   Reconfigure Blender: cd ~/blender-git/build-cmake ; cmake ."
echo "   Build Blender: cd ~/blender-git/build-cmake ; make"
echo ""
