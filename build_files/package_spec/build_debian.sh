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
# value may be formatted: 35042:35051M
BLENDER_REVISION=$(svnversion | cut -d: -f2 | tr -dc 0-9)

blender_version=$(grep BLENDER_VERSION source/blender/blenkernel/BKE_blender.h | tr -dc 0-9)
blender_version_char=$(sed -ne 's/.*BLENDER_VERSION_CHAR.*\([a-z]\)$/\1/p' source/blender/blenkernel/BKE_blender.h)
BLENDER_VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)

# map the version a -> 1, to conform to debian naming convention
# not to be confused with blender's internal subversions
if [ "$blender_version_char" ]; then
    BLENDER_VERSION=${BLENDER_VERSION}.$(expr index abcdefghijklmnopqrstuvwxyz $blender_version_char)
fi

DEB_VERSION=${BLENDER_VERSION}+svn${BLENDER_REVISION}-bf

# update debian/changelog
dch -b -v $DEB_VERSION "New upstream SVN snapshot."


# run the rules makefile
rm -rf get-orig-source
debian/rules get-orig-source SVN_URL=.
mv *.gz ../

# build the package
debuild -i -us -uc -b


# remove temp dir
rm -rf debian
