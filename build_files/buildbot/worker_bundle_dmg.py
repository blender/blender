#!/usr/bin/env python3

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import argparse
import re
import shutil
import subprocess
import sys
import time

from pathlib import Path
from tempfile import TemporaryDirectory, NamedTemporaryFile
from typing import List

BUILDBOT_DIRECTORY = Path(__file__).absolute().parent
CODESIGN_SCRIPT = BUILDBOT_DIRECTORY / 'worker_codesign.py'
BLENDER_GIT_ROOT_DIRECTORY = BUILDBOT_DIRECTORY.parent.parent
DARWIN_DIRECTORY = BLENDER_GIT_ROOT_DIRECTORY / 'release' / 'darwin'


# Extra size which is added on top of actual files size when estimating size
# of destination DNG.
EXTRA_DMG_SIZE_IN_BYTES = 800 * 1024 * 1024

################################################################################
# Common utilities


def get_directory_size(root_directory: Path) -> int:
    """
    Get size of directory on disk
    """

    total_size = 0
    for file in root_directory.glob('**/*'):
        total_size += file.lstat().st_size
    return total_size


################################################################################
# DMG bundling specific logic

def create_argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'source_dir',
        type=Path,
        help='Source directory which points to either existing .app bundle'
             'or to a directory with .app bundles.')
    parser.add_argument(
        '--background-image',
        type=Path,
        help="Optional background picture which will be set on the DMG."
             "If not provided default Blender's one is used.")
    parser.add_argument(
        '--volume-name',
        type=str,
        help='Optional name of a volume which will be used for DMG.')
    parser.add_argument(
        '--dmg',
        type=Path,
        help='Optional argument which points to a final DMG file name.')
    parser.add_argument(
        '--applescript',
        type=Path,
        help="Optional path to applescript to set up folder looks of DMG."
             "If not provided default Blender's one is used.")
    return parser


def collect_app_bundles(source_dir: Path) -> List[Path]:
    """
    Collect all app bundles which are to be put into DMG

    If the source directory points to FOO.app it will be the only app bundle
    packed.

    Otherwise all .app bundles from given directory are placed to a single
    DMG.
    """

    if source_dir.name.endswith('.app'):
        return [source_dir]

    app_bundles = []
    for filename in source_dir.glob('*'):
        if not filename.is_dir():
            continue
        if not filename.name.endswith('.app'):
            continue

        app_bundles.append(filename)

    return app_bundles


def collect_and_log_app_bundles(source_dir: Path) -> List[Path]:
    app_bundles = collect_app_bundles(source_dir)

    if not app_bundles:
        print('No app bundles found for packing')
        return

    print(f'Found {len(app_bundles)} to pack:')
    for app_bundle in app_bundles:
        print(f'- {app_bundle}')

    return app_bundles


def estimate_dmg_size(app_bundles: List[Path]) -> int:
    """
    Estimate size of DMG to hold requested app bundles

    The size is based on actual size of all files in all bundles plus some
    space to compensate for different size-on-disk plus some space to hold
    codesign signatures.

    Is better to be on a high side since the empty space is compressed, but
    lack of space might cause silent failures later on.
    """

    app_bundles_size = 0
    for app_bundle in app_bundles:
        app_bundles_size += get_directory_size(app_bundle)

    return app_bundles_size + EXTRA_DMG_SIZE_IN_BYTES


def copy_app_bundles_to_directory(app_bundles: List[Path],
                                  directory: Path) -> None:
    """
    Copy all bundles to a given directory

    This directory is what the DMG will be created from.
    """
    for app_bundle in app_bundles:
        print(f'Copying {app_bundle.name}...')
        shutil.copytree(app_bundle, directory / app_bundle.name)


def get_main_app_bundle(app_bundles: List[Path]) -> Path:
    """
    Get application bundle main for the installation
    """
    return app_bundles[0]


def create_dmg_image(app_bundles: List[Path],
                     dmg_filepath: Path,
                     volume_name: str) -> None:
    """
    Create DMG disk image and put app bundles in it

    No DMG configuration or codesigning is happening here.
    """

    if dmg_filepath.exists():
        print(f'Removing existing writable DMG {dmg_filepath}...')
        dmg_filepath.unlink()

    print('Preparing directory with app bundles for the DMG...')
    with TemporaryDirectory(prefix='blender-dmg-content-') as content_dir_str:
        # Copy all bundles to a clean directory.
        content_dir = Path(content_dir_str)
        copy_app_bundles_to_directory(app_bundles, content_dir)

        # Estimate size of the DMG.
        dmg_size = estimate_dmg_size(app_bundles)
        print(f'Estimated DMG size: {dmg_size:,} bytes.')

        # Create the DMG.
        print(f'Creating writable DMG {dmg_filepath}')
        command = ('hdiutil',
                   'create',
                   '-size', str(dmg_size),
                   '-fs', 'HFS+',
                   '-srcfolder', content_dir,
                   '-volname', volume_name,
                   '-format', 'UDRW',
                   dmg_filepath)
        subprocess.run(command)


def get_writable_dmg_filepath(dmg_filepath: Path):
    """
    Get file path for writable DMG image
    """
    parent = dmg_filepath.parent
    return parent / (dmg_filepath.stem + '-temp.dmg')


def mount_readwrite_dmg(dmg_filepath: Path) -> None:
    """
    Mount writable DMG

    Mounting point would be /Volumes/<volume name>
    """

    print(f'Mounting read-write DMG ${dmg_filepath}')
    command = ('hdiutil',
               'attach', '-readwrite',
               '-noverify',
               '-noautoopen',
               dmg_filepath)
    subprocess.run(command)


def get_mount_directory_for_volume_name(volume_name: str) -> Path:
    """
    Get directory under which the volume will be mounted
    """

    return Path('/Volumes') / volume_name


def eject_volume(volume_name: str) -> None:
    """
    Eject given volume, if mounted
    """
    mount_directory = get_mount_directory_for_volume_name(volume_name)
    if not mount_directory.exists():
        return
    mount_directory_str = str(mount_directory)

    print(f'Ejecting volume {volume_name}')

    # Figure out which device to eject.
    mount_output = subprocess.check_output(['mount']).decode()
    device = ''
    for line in mount_output.splitlines():
        if f'on {mount_directory_str} (' not in line:
            continue
        tokens = line.split(' ', 3)
        if len(tokens) < 3:
            continue
        if tokens[1] != 'on':
            continue
        if device:
            raise Exception(
                f'Multiple devices found for mounting point {mount_directory}')
        device = tokens[0]

    if not device:
        raise Exception(
            f'No device found for mounting point {mount_directory}')

    print(f'{mount_directory} is mounted as device {device}, ejecting...')
    subprocess.run(['diskutil', 'eject', device])


def copy_background_if_needed(background_image_filepath: Path,
                              mount_directory: Path) -> None:
    """
    Copy background to the DMG

    If the background image is not specified it will not be copied.
    """

    if not background_image_filepath:
        print('No background image provided.')
        return

    print(f'Copying background image {background_image_filepath}')

    destination_dir = mount_directory / '.background'
    destination_dir.mkdir(exist_ok=True)

    destination_filepath = destination_dir / background_image_filepath.name
    shutil.copy(background_image_filepath, destination_filepath)


def create_applications_link(mount_directory: Path) -> None:
    """
    Create link to /Applications in the given location
    """

    print('Creating link to /Applications')

    command = ('ln', '-s', '/Applications', mount_directory / ' ')
    subprocess.run(command)


def run_applescript(applescript: Path,
                    volume_name: str,
                    app_bundles: List[Path],
                    background_image_filepath: Path) -> None:
    """
    Run given applescript to adjust look and feel of the DMG
    """

    main_app_bundle = get_main_app_bundle(app_bundles)

    with NamedTemporaryFile(
            mode='w', suffix='.applescript') as temp_applescript:
        print('Adjusting applescript for volume name...')
        # Adjust script to the specific volume name.
        with open(applescript, mode='r') as input:
            for line in input.readlines():
                stripped_line = line.strip()
                if stripped_line.startswith('tell disk'):
                    line = re.sub('tell disk ".*"',
                                  f'tell disk "{volume_name}"',
                                  line)
                elif stripped_line.startswith('set background picture'):
                    if not background_image_filepath:
                        continue
                    else:
                        background_image_short = \
                            '.background:' + background_image_filepath.name
                        line = re.sub('to file ".*"',
                                      f'to file "{background_image_short}"',
                                      line)
                line = line.replace('blender.app', main_app_bundle.name)
                temp_applescript.write(line)

        temp_applescript.flush()

        print('Running applescript...')
        command = ('osascript',  temp_applescript.name)
        subprocess.run(command)

        print('Waiting for applescript...')

        # NOTE: This is copied from bundle.sh. The exact reason for sleep is
        # still remained a mystery.
        time.sleep(5)


def codesign(subject: Path):
    """
    Codesign file or directory

    NOTE: For DMG it will also notarize.
    """

    command = (CODESIGN_SCRIPT, subject)
    subprocess.run(command)


def codesign_app_bundles_in_dmg(mount_directory: str) -> None:
    """
    Code sign all binaries and bundles in the mounted directory
    """

    print(f'Codesigning all app bundles in {mount_directory}')
    codesign(mount_directory)


def codesign_and_notarize_dmg(dmg_filepath: Path) -> None:
    """
    Run codesign and notarization pipeline on the DMG
    """

    print(f'Codesigning and notarizing DMG {dmg_filepath}')
    codesign(dmg_filepath)


def compress_dmg(writable_dmg_filepath: Path,
                 final_dmg_filepath: Path) -> None:
    """
    Compress temporary read-write DMG
    """
    command = ('hdiutil', 'convert',
               writable_dmg_filepath,
               '-format', 'UDZO',
               '-o', final_dmg_filepath)

    if final_dmg_filepath.exists():
        print(f'Removing old compressed DMG {final_dmg_filepath}')
        final_dmg_filepath.unlink()

    print('Compressing disk image...')
    subprocess.run(command)


def create_final_dmg(app_bundles: List[Path],
                     dmg_filepath: Path,
                     background_image_filepath: Path,
                     volume_name: str,
                     applescript: Path) -> None:
    """
    Create DMG with all app bundles

    Will take care configuring background, signing all binaries and app bundles
    and notarizing the DMG.
    """

    print('Running all routines to create final DMG')

    writable_dmg_filepath = get_writable_dmg_filepath(dmg_filepath)
    mount_directory = get_mount_directory_for_volume_name(volume_name)

    # Make sure volume is not mounted.
    # If it is mounted it will prevent removing old DMG files and could make
    # it so app bundles are copied to the wrong place.
    eject_volume(volume_name)

    create_dmg_image(app_bundles, writable_dmg_filepath, volume_name)

    mount_readwrite_dmg(writable_dmg_filepath)

    # Run codesign first, prior to copying amything else.
    #
    # This allows to recurs into the content of bundles without worrying about
    # possible interfereice of Application symlink.
    codesign_app_bundles_in_dmg(mount_directory)

    copy_background_if_needed(background_image_filepath, mount_directory)
    create_applications_link(mount_directory)
    run_applescript(applescript, volume_name, app_bundles,
                    background_image_filepath)

    print('Ejecting read-write DMG image...')
    eject_volume(volume_name)

    compress_dmg(writable_dmg_filepath, dmg_filepath)
    writable_dmg_filepath.unlink()

    codesign_and_notarize_dmg(dmg_filepath)


def ensure_dmg_extension(filepath: Path) -> Path:
    """
    Make sure given file have .dmg extension
    """

    if filepath.suffix != '.dmg':
        return filepath.with_suffix(f'{filepath.suffix}.dmg')
    return filepath


def get_dmg_filepath(requested_name: Path, app_bundles: List[Path]) -> Path:
    """
    Get full file path for the final DMG image

    Will use the provided one when possible, otherwise will deduct it from
    app bundles.

    If the name is deducted, the DMG is stored in the current directory.
    """

    if requested_name:
        return ensure_dmg_extension(requested_name.absolute())

    # TODO(sergey): This is not necessarily the main one.
    main_bundle = app_bundles[0]
    # Strip .app from the name
    return Path(main_bundle.name[:-4] + '.dmg').absolute()


def get_background_image(requested_background_image: Path) -> Path:
    """
    Get effective filepath for the background image
    """

    if requested_background_image:
        return requested_background_image.absolute()

    return DARWIN_DIRECTORY / 'background.tif'


def get_applescript(requested_applescript: Path) -> Path:
    """
    Get effective filepath for the applescript
    """

    if requested_applescript:
        return requested_applescript.absolute()

    return DARWIN_DIRECTORY / 'blender.applescript'


def get_volume_name_from_dmg_filepath(dmg_filepath: Path) -> str:
    """
    Deduct volume name from the DMG path

    Will use first part of the DMG file name prior to dash.
    """

    tokens = dmg_filepath.stem.split('-')
    words = tokens[0].split()

    return ' '.join(word.capitalize() for word in words)


def get_volume_name(requested_volume_name: str,
                    dmg_filepath: Path) -> str:
    """
    Get effective name for DMG volume
    """

    if requested_volume_name:
        return requested_volume_name

    return get_volume_name_from_dmg_filepath(dmg_filepath)


def main():
    parser = create_argument_parser()
    args = parser.parse_args()

    # Get normalized input parameters.
    source_dir = args.source_dir.absolute()
    background_image_filepath = get_background_image(args.background_image)
    applescript = get_applescript(args.applescript)

    app_bundles = collect_and_log_app_bundles(source_dir)
    if not app_bundles:
        return

    dmg_filepath = get_dmg_filepath(args.dmg, app_bundles)
    volume_name = get_volume_name(args.volume_name, dmg_filepath)

    print(f'Will produce DMG "{dmg_filepath.name}" (without quotes)')

    create_final_dmg(app_bundles,
                     dmg_filepath,
                     background_image_filepath,
                     volume_name,
                     applescript)


if __name__ == "__main__":
    main()
