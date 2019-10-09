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

# <pep8 compliant>

# Runs on buildbot slave, creating a release package using the build
# system and zipping it into buildbot_upload.zip. This is then uploaded
# to the master in the next buildbot step.

import buildbot_utils
import os
import sys

def get_package_name(builder, platform=None):
    info = buildbot_utils.VersionInfo(builder)

    package_name = 'blender-' + info.full_version
    if platform:
      package_name += '-' + platform
    if builder.branch != 'master' and info.is_development_build:
        package_name = builder.branch + "-" + package_name

    return package_name

def create_buildbot_upload_zip(builder, package_files):
    import zipfile

    buildbot_upload_zip = os.path.join(builder.upload_dir, "buildbot_upload.zip")
    if os.path.exists(buildbot_upload_zip):
        os.remove(buildbot_upload_zip)

    try:
        z = zipfile.ZipFile(buildbot_upload_zip, "w", compression=zipfile.ZIP_STORED)
        for filepath, filename in package_files:
            z.write(filepath, arcname=filename)
        z.close()
    except Exception as ex:
        sys.stderr.write('Create buildbot_upload.zip failed: ' + str(ex) + '\n')
        sys.exit(1)

def create_tar_bz2(src, dest, package_name):
    # One extra to remove leading os.sep when cleaning root for package_root
    ln = len(src) + 1
    flist = list()

    # Create list of tuples containing file and archive name
    for root, dirs, files in os.walk(src):
        package_root = os.path.join(package_name, root[ln:])
        flist.extend([(os.path.join(root, file), os.path.join(package_root, file)) for file in files])

    import tarfile
    package = tarfile.open(dest, 'w:bz2')
    for entry in flist:
        package.add(entry[0], entry[1], recursive=False)
    package.close()

def cleanup_files(dirpath, extension):
    for f in os.listdir(dirpath):
        filepath = os.path.join(dirpath, f)
        if os.path.isfile(filepath) and f.endswith(extension):
            os.remove(filepath)


def pack_mac(builder):
    info = buildbot_utils.VersionInfo(builder)

    os.chdir(builder.build_dir)
    cleanup_files(builder.build_dir, '.dmg')

    package_name = get_package_name(builder, 'macOS')
    package_filename = package_name + '.dmg'
    package_filepath = os.path.join(builder.build_dir, package_filename)

    release_dir = os.path.join(builder.blender_dir, 'release', 'darwin')
    bundle_sh = os.path.join(release_dir, 'bundle.sh')

    command = [bundle_sh]
    command += ['--source', builder.install_dir]
    command += ['--dmg', package_filepath]
    if info.is_development_build:
        background_image = os.path.join(release_dir, 'buildbot', 'background.tif')
        command += ['--background-image', background_image]
    buildbot_utils.call(command)

    create_buildbot_upload_zip(builder, [(package_filepath, package_filename)])


def pack_win(builder):
    info = buildbot_utils.VersionInfo(builder)

    os.chdir(builder.build_dir)
    cleanup_files(builder.build_dir, '.zip')

    # CPack will add the platform name
    cpack_name = get_package_name(builder, None)
    package_name = get_package_name(builder, 'windows' + str(builder.bits))

    command = ['cmake', '-DCPACK_OVERRIDE_PACKAGENAME:STRING=' + cpack_name, '.']
    buildbot_utils.call(builder.command_prefix + command)
    command = ['cpack', '-G', 'ZIP']
    buildbot_utils.call(builder.command_prefix + command)

    package_filename = package_name + '.zip'
    package_filepath = os.path.join(builder.build_dir, package_filename)
    package_files = [(package_filepath, package_filename)]

    if info.version_cycle == 'release':
        # Installer only for final release builds, otherwise will get
        # 'this product is already installed' messages.
        command = ['cpack', '-G', 'WIX']
        buildbot_utils.call(builder.command_prefix + command)

        package_filename = package_name + '.msi'
        package_filepath = os.path.join(builder.build_dir, package_filename)
        package_files += [(package_filepath, package_filename)]

    create_buildbot_upload_zip(builder, package_files)


def pack_linux(builder):
    blender_executable = os.path.join(builder.install_dir, 'blender')

    info = buildbot_utils.VersionInfo(builder)
    blender_glibc = builder.name.split('_')[1]
    blender_arch = 'x86_64'

    # Strip all unused symbols from the binaries
    print("Stripping binaries...")
    buildbot_utils.call(builder.command_prefix + ['strip', '--strip-all', blender_executable])

    print("Stripping python...")
    py_target = os.path.join(builder.install_dir, info.version)
    buildbot_utils.call(builder.command_prefix + ['find', py_target, '-iname', '*.so', '-exec', 'strip', '-s', '{}', ';'])

    # Construct package name
    platform_name = 'linux-' + blender_glibc + '-' + blender_arch
    package_name = get_package_name(builder, platform_name)
    package_filename = package_name + ".tar.bz2"

    print("Creating .tar.bz2 archive")
    package_filepath = builder.install_dir + '.tar.bz2'
    create_tar_bz2(builder.install_dir, package_filepath, package_name)

    # Create buildbot_upload.zip
    create_buildbot_upload_zip(builder, [(package_filepath, package_filename)])


if __name__ == "__main__":
    builder = buildbot_utils.create_builder_from_arguments()

    # Make sure install directory always exists
    os.makedirs(builder.install_dir, exist_ok=True)

    if builder.platform == 'mac':
        pack_mac(builder)
    elif builder.platform == 'win':
        pack_win(builder)
    elif builder.platform == 'linux':
        pack_linux(builder)
