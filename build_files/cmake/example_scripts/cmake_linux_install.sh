#!/bin/sh

# This shell script checks out and compiles blender, tested on ubuntu 10.04
# assumes you have dependancies installed alredy

# See this page for more info:
#  http://wiki.blender.org/index.php/Dev:2.5/Doc/Building_Blender/Linux/Generic_Distro/CMake

# grab blender
mkdir ~/blender-svn 
cd ~/blender-svn
svn co https://svn.blender.org/svnroot/bf-blender/trunk/blender

# create cmake dir
mkdir ~/blender-svn/build-cmake
cd ~/blender-svn/build-cmake

# cmake without copying files for fast rebuilds
# the files from svn will be used in place
cmake ../blender

# make blender, will take some time
make

# link the binary to blenders source directory to run quickly
ln -s ~/blender-svn/build-cmake/bin/blender ~/blender-svn/blender/blender.bin

# useful info
echo ""
echo "* Useful Commands *"
echo "   Run Blender: ~/blender-svn/blender/blender.bin"
echo "   Update Blender: svn up ~/blender-svn/blender"
echo "   Reconfigure Blender: cd ~/blender-svn/build-cmake ; cmake ."
echo "   Build Blender: cd ~/blender-svn/build-cmake ; make"
echo ""


