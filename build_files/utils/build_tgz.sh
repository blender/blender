#!/bin/sh

# This script can run from any location,
# output is created in the $CWD

BASE_DIR="$PWD"

blender_srcdir=$(dirname -- $0)/../..
blender_version=$(grep "BLENDER_VERSION\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_version_char=$(grep "BLENDER_VERSION_CHAR\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_version_cycle=$(grep "BLENDER_VERSION_CYCLE\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')
blender_subversion=$(grep "BLENDER_SUBVERSION\s" "$blender_srcdir/source/blender/blenkernel/BKE_blender.h" | awk '{print $3}')

if [ "$blender_version_cycle" = "release" ] ; then
	VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)$blender_version_char
else
	VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)_$blender_subversion
fi

MANIFEST="blender-$VERSION-manifest.txt"
TARBALL="blender-$VERSION.tar.gz"

cd "$blender_srcdir"

# Build master list
echo -n "Building manifest of files:  \"$BASE_DIR/$MANIFEST\" ..."
git ls-files > $BASE_DIR/$MANIFEST

# Enumerate submodules
for lcv in $(git submodule | cut -f2 -d" "); do
	cd "$BASE_DIR"
	cd "$blender_srcdir/$lcv"
	git ls-files | awk '$0="'"$lcv"/'"$0' >> $BASE_DIR/$MANIFEST
	cd "$BASE_DIR"
done
echo "OK"


# Create the tarball
cd "$blender_srcdir"
echo -n "Creating archive:            \"$BASE_DIR/$TARBALL\" ..."
GZIP=-9 tar --transform "s,^,blender-$VERSION/,g" -zcf "$BASE_DIR/$TARBALL" -T "$BASE_DIR/$MANIFEST"
echo "OK"


# Create checksum file
cd "$BASE_DIR"
echo -n "Createing checksum:          \"$BASE_DIR/$TARBALL.md5sum\" ..."
md5sum "$TARBALL" > "$TARBALL.md5sum"
echo "OK"


# Cleanup
echo -n "Cleaning up ..."
rm "$BASE_DIR/$MANIFEST"
echo "OK"

echo "Done!"
