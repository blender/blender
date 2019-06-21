#!/usr/bin/env bash

# create blender distribution dmg

# check that we have all needed tools

for i in osascript git codesign hdiutil xcrun ; do
    if [ ! -x "$(which ${i})" ]; then
	echo "Unable to execute command $i, macOS broken?"
	exit 1
    fi
done

# some defaults settings

_scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
_volname="Blender"
_tmpdir="$(mktemp -d)"
_tmpdmg="/tmp/blender-tmp.dmg"
BACKGROUND_IMAGE="${_scriptdir}/background.tif"
MOUNT_DIR="/Volumes/${_volname}"

# handle arguments

while [[ $# -gt 0 ]]; do
    key=$1
    case $key in
	-s|--source)
	    SRC_DIR="$2"
	    shift
	    shift
	    ;;
	-d|--dmg)
	    DEST_DMG="$2"
	    shift
	    shift
	    ;;
	-b|--bundle-id)
	    N_BUNDLE_ID="$2"
	    shift
	    shift
	    ;;
	-u|--username)
	    N_USERNAME="$2"
	    shift
	    shift
	    ;;
	-p|--password)
	    N_PASSWORD="$2"
	    shift
	    shift
	    ;;
	-c|--codesign)
	    C_CERT="$2"
	    shift
	    shift
	    ;;
	-h|--help)
	    echo "Usage:"
	    echo " $(basename "$0") --source DIR --dmg IMAGENAME "
	    echo "    optional arguments:"
	    echo "    --codesign <certname>"
	    echo "    --username <username>"
	    echo "    --password <password>"
	    echo "    --bundle-id <bundleid>"
	    echo " Check https://developer.apple.com/documentation/security/notarizing_your_app_before_distribution/customizing_the_notarization_workflow "
	    exit 1
	    ;;
    esac
done

if [ ! -d "${SRC_DIR}/Blender.app" ]; then
    echo "use --source parameter to set source directory where Blender.app can be found"
    exit 1
fi

if [ -z "${DEST_DMG}" ]; then
    echo "use --dmg parameter to set output dmg name"
    exit 1
fi

# destroy destination dmg if there is any. be warned.

test -f "${DEST_DMG}" && rm "${DEST_DMG}"
if [ -d "${MOUNT_DIR}" ]; then
    echo -n "Ejecting existing blender volume.."
    DEV_FILE=$(mount | grep "${MOUNT_DIR}" | awk '{ print $1 }')
    diskutil eject "${DEV_FILE}" || exit 1
    echo
fi

# let's go.

echo -n "Copying Blender.app..."
cp -r "${SRC_DIR}/Blender.app" "${_tmpdir}/" || exit 1
echo

# Create the disk image

_ds=$(du -sh ${_tmpdir} | awk -F'[^0-9]*' '$0=$1') # directory size
_is=$(echo "${_ds}" + 200 | bc) # image size with extra 200 ! (why on earth!) for codesign to work
echo
echo -n "Creating disk image of size ${_is}M.."
test -f "${_tmpdmg}" && rm "${_tmpdmg}"
hdiutil create -size "${_is}m" -fs HFS+ -srcfolder "${_tmpdir}" -volname "${_volname}" -format UDRW "${_tmpdmg}"

echo "Mounting readwrite image..."
hdiutil attach -readwrite -noverify -noautoopen "${_tmpdmg}"

echo "Setting background picture.."
if ! test -z "${BACKGROUND_IMAGE}"; then
    echo "Copying background image ..."
    test -d "${MOUNT_DIR}/.background" || mkdir "${MOUNT_DIR}/.background"
    BACKGROUND_IMAGE_NAME=$(basename "${BACKGROUND_IMAGE}")
    cp "${BACKGROUND_IMAGE}" "${MOUNT_DIR}/.background/${BACKGROUND_IMAGE_NAME}"
fi

# echo "Creating link to /Applications ..."
ln -s /Applications "${MOUNT_DIR}/Applications"
echo "Renaming Applications to empty string."
mv ${MOUNT_DIR}/Applications "${MOUNT_DIR}/ "

echo "Running applescript to set folder looks ..."
cat "${_scriptdir}/blender.applescript" | osascript

echo "Waiting after applescript ..."
sleep 5

if [ ! -z "${C_CERT}" ]; then
    # codesigning seems to be thingie. all libs and binaries need to be
    # signed separately. todo: use some find magic to find those
    echo -n "Codesigning..."
    codesign --timestamp --options runtime --sign "${C_CERT}" "${MOUNT_DIR}/Blender.app/Contents/Resources/2.80/python/bin/python3.7m"
    codesign --timestamp --options runtime --sign "${C_CERT}" "${MOUNT_DIR}/Blender.app/Contents/Resources/2.80/python/lib/python3.7/site-packages/libextern_draco.dylib"
    codesign --timestamp --options runtime --sign "${C_CERT}" "${MOUNT_DIR}/Blender.app/Contents/Resources/lib/libomp.dylib"
    codesign --timestamp --options runtime --sign "${C_CERT}" "${MOUNT_DIR}/Blender.app"
    echo
else
    echo "No codesigning cert given, skipping..."
fi


echo "Unmounting rw disk image ..."
# need to eject dev files to remove /dev files and free .dmg for converting
DEV_FILE=$(mount | grep "${MOUNT_DIR}" | awk '{ print $1 }')
diskutil eject "${DEV_FILE}"

sleep 3

echo "Compressing disk image ..."
hdiutil convert "${_tmpdmg}" -format UDZO -o "${DEST_DMG}"

# codesign the dmg

if [ ! -z "${C_CERT}" ]; then
    echo -n "Codesigning dmg..."
    codesign --timestamp --force --sign "${C_CERT}" "${DEST_DMG}"
    echo
fi

# cleanup

rm -rf "${_tmpdir}"
rm "${_tmpdmg}"

# send notarization
if [ ! -z "${N_USERNAME}" ] && [ ! -z "${N_PASSWORD}" ] && [ ! -z "${N_BUNDLE_ID}" ]; then
    echo -n "Sending ${DEST_DMG} for notarization..."
    _tmpout=$(mktemp)
    xcrun altool --notarize-app -f "${DEST_DMG}" --primary-bundle-id "${N_BUNDLE_ID}" --username "${N_USERNAME}" --password "${N_PASSWORD}" >${_tmpout} 2>&1

    # check the request uuid

    _requuid=$(cat "${_tmpout}" | grep "RequestUUID" | awk '{ print $3 }')
    echo "RequestUUID: ${_requuid}"
    if [ ! -z "${_requuid}" ]; then
	echo "Waiting for notarization to be complete.."
	for c in {20..0};do
	    sleep 600
	    xcrun altool --notarization-info "${_requuid}" --username "${N_USERNAME}" --password "${N_PASSWORD}" >${_tmpout} 2>&1
	    _status=$(cat "${_tmpout}" | grep "Status:" | awk '{ print $2 }')
	    if [ "${_status}" == "invalid" ]; then
		echo "Got invalid notarization!"
		break;
	    fi

	    if [ "${_status}" == "success" ]; then
		echo -n "Notarization successful! Stapling..."
		xcrun stapler staple -v "${DEST_DMG}"
		break;
	    fi
	    echo "Notarization in progress, waiting..."
	done
    else
	echo "Error getting RequestUUID, notarization unsuccessful"
    fi
else
    echo "No notarization credentials supplied, skipping..."
fi

echo "..done. You should have ${DEST_DMG} ready to upload"
