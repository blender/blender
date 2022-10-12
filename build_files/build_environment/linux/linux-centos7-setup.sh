#!/bin/sh

set -e

if [ `id -u` -ne 0 ]; then
   echo "This script must be run as root"
   exit 1
fi

# yum-config-manager does not come in the default minimal install,
# so make sure it is installed and available.
yum -y update
yum -y install yum-utils

# Install all the packages needed for a new toolchain.
#
# NOTE: Keep this separate from the packages install, since otherwise
# older toolchain will be installed.
yum -y update
yum -y install epel-release
yum -y install centos-release-scl
yum -y install devtoolset-9

# Install packages needed for Blender's dependencies.
yum -y install -y  \
    git subversion bzip2 tar cmake3 patch make autoconf automake libtool  \
    meson ninja-build \
    libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel  \
    libX11-devel libXt-devel  \
    mesa-libEGL-devel mesa-libGL-devel mesa-libGLU-devel \
    zlib-devel  \
    rubygem-asciidoctor \
    wget tcl yasm python36 python-setuptools bison flex \
    ncurses-devel \
    wayland-devel libwayland-client libwayland-server \


# Dependencies for Mesa
yum -y install expat-devel
python3 -m pip install mako

# Dependencies for pip (needed for buildbot-worker).
yum -y install python36-pip python36-devel

# Dependencies for asound.
yum -y install -y  \
    alsa-lib-devel pulseaudio-libs-devel

alternatives --install /usr/local/bin/cmake cmake /usr/bin/cmake 10  \
    --slave /usr/local/bin/ctest ctest /usr/bin/ctest  \
    --slave /usr/local/bin/cpack cpack /usr/bin/cpack  \
    --slave /usr/local/bin/ccmake ccmake /usr/bin/ccmake  \
    --family cmake

alternatives --install /usr/local/bin/cmake cmake /usr/bin/cmake3 20  \
    --slave /usr/local/bin/ctest ctest /usr/bin/ctest3  \
    --slave /usr/local/bin/cpack cpack /usr/bin/cpack3  \
    --slave /usr/local/bin/ccmake ccmake /usr/bin/ccmake3  \
    --family cmake

alternatives --install /usr/local/bin/cmake cmake /usr/bin/cmake3 20  \
    --slave /usr/local/bin/ctest ctest /usr/bin/ctest3  \
    --slave /usr/local/bin/cpack cpack /usr/bin/cpack3  \
    --slave /usr/local/bin/ccmake ccmake /usr/bin/ccmake3  \
    --family cmake
