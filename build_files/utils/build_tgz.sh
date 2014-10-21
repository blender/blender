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
	SUBMODULE_EXCLUDE="^\(release/scripts/addons_contrib\)$"
else
	VERSION=$(expr $blender_version / 100).$(expr $blender_version % 100)_$blender_subversion
	SUBMODULE_EXCLUDE="^$"  # dummy regex
fi

MANIFEST="blender-$VERSION-manifest.txt"
TARBALL="blender-$VERSION.tar.gz"

cd "$blender_srcdir"

# not so nice, but works
FILTER_FILES_PY="import os, sys; [print(l[:-1]) for l in sys.stdin.readlines() if os.path.isfile(l[:-1])]"

# Build master list
echo -n "Building manifest of files:  \"$BASE_DIR/$MANIFEST\" ..."
git ls-files | python3 -c "$FILTER_FILES_PY" > $BASE_DIR/$MANIFEST

# Enumerate submodules
for lcv in $(git submodule | awk '{print $2}' | grep -v "$SUBMODULE_EXCLUDE"); do
	cd "$BASE_DIR"
	cd "$blender_srcdir/$lcv"
	git ls-files | python3 -c "$FILTER_FILES_PY" | awk '$0="'"$lcv"/'"$0' >> $BASE_DIR/$MANIFEST
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
