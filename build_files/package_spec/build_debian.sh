#!/bin/sh
# Builds a debian package from SVN source.


# this needs to run in the root dir.
cd $(dirname $0)/../../
ln -s $PWD/build_files/package_spec/debian $PWD/debian


# Get values from blender to use in debian/changelog.
BLENDER_REVISION=$(svnversion)

blender_srcdir=$PWD
blender_version=$(grep BLENDER_VERSION $blender_srcdir/source/blender/blenkernel/BKE_blender.h | tr -dc 0-9)
BLENDER_VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)

# replace changelog value
svn revert debian/changelog
sed -i 's/<VER>/'$BLENDER_VERSION'/g' debian/changelog
sed -i 's/<REV>/'$BLENDER_REVISION'/g' debian/changelog


# run the rules makefile
debian/rules get-orig-source SVN_URL=.
mv *.gz ../

# build the package
debuild -i -us -uc -b


# remove symlink
rm debian
