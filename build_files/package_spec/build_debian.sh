#!/bin/sh
# Builds a debian package from SVN source.
#
# For paralelle builds use:
#  DEB_BUILD_OPTIONS="parallel=5" sh build_files/package_spec/build_debian.sh

# this needs to run in the root dir.
cd $(dirname $0)/../../
rm -rf debian
cp -a build_files/package_spec/debian .


# Get values from blender to use in debian/changelog.
BLENDER_REVISION=$(svnversion)

blender_srcdir=$PWD
blender_version=$(grep BLENDER_VERSION $blender_srcdir/source/blender/blenkernel/BKE_blender.h | tr -dc 0-9)
BLENDER_VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)
DEB_VERSION=${BLENDER_VERSION}+svn${BLENDER_REVISION}-bf

# update debian/changelog
dch -b -v $DEB_VERSION "New upstream SVN snapshot."


# run the rules makefile
debian/rules get-orig-source SVN_URL=.
mv *.gz ../

# build the package
debuild -i -us -uc -b


# remove temp dir
rm -rf debian
