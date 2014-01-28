#!/bin/sh

# This shell script checks out and compiles blender, tested on ubuntu 10.04
# assumes you have dependencies installed already

# See this page for more info:
#   http://wiki.blender.org/index.php/Dev:Doc/Building_Blender/Linux/Generic_Distro/CMake

# grab blender
mkdir ~/blender-git 
cd ~/blender-git

git clone http://git.blender.org/blender.git
cd blender
git submodule update --init --recursive
git submodule foreach git checkout master
git submodule foreach git pull --rebase origin master

# create cmake dir
mkdir ~/blender-git/build-cmake
cd ~/blender-git/build-cmake

# cmake without copying files for fast rebuilds
# the files from git will be used in place
cmake ../blender

# make blender, will take some time
make

# link the binary to blenders source directory to run quickly
ln -s ~/blender-git/build-cmake/bin/blender ~/blender-git/blender/blender.bin

# useful info
echo ""
echo "* Useful Commands *"
echo "   Run Blender: ~/blender-git/blender/blender.bin"
echo "   Update Blender: git pull --rebase; git submodule foreach git pull --rebase origin master"
echo "   Reconfigure Blender: cd ~/blender-git/build-cmake ; cmake ."
echo "   Build Blender: cd ~/blender-git/build-cmake ; make"
echo ""

