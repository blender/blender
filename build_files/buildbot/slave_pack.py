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

import os
import subprocess
import sys
import zipfile

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]
# Never write branch if it is master.
branch = sys.argv[2] if (len(sys.argv) >= 3 and sys.argv[2] != 'master') else ''

blender_dir = os.path.join('..', 'blender.git')
build_dir = os.path.join('..', 'build', builder)
install_dir = os.path.join('..', 'install', builder)
buildbot_upload_zip = os.path.abspath(os.path.join(os.path.dirname(install_dir), "buildbot_upload.zip"))

upload_filename = None  # Name of the archive to be uploaded
                        # (this is the name of archive which will appear on the
                        # download page)
upload_filepath = None  # Filepath to be uploaded to the server
                        # (this folder will be packed)


def parse_header_file(filename, define):
    import re
    regex = re.compile("^#\s*define\s+%s\s+(.*)" % define)
    with open(filename, "r") as file:
        for l in file:
            match = regex.match(l)
            if match:
                return match.group(1)
    return None


# Make sure install directory always exists
if not os.path.exists(install_dir):
    os.makedirs(install_dir)


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


if builder.find('cmake') != -1:
    # CMake
    if 'win' in builder or 'mac' in builder:
        os.chdir(build_dir)

        files = [f for f in os.listdir('.') if os.path.isfile(f) and f.endswith('.zip')]
        for f in files:
            os.remove(f)
        retcode = subprocess.call(['cpack', '-G', 'ZIP'])
        result_file = [f for f in os.listdir('.') if os.path.isfile(f) and f.endswith('.zip')][0]

        # TODO(sergey): Such magic usually happens in SCon's packaging but we don't have it
        # in the CMake yet. For until then we do some magic here.
        tokens = result_file.split('-')
        blender_version = tokens[1].split('.')
        blender_full_version = '.'.join(blender_version[0:2])
        git_hash = tokens[2].split('.')[1]
        platform = builder.split('_')[0]
        if platform == 'mac':
            # Special exception for OSX
            platform = 'OSX-10.9-'
            if builder.endswith('x86_64_10_9_cmake'):
                platform += 'x86_64'
        if builder.endswith('vc2015'):
            platform += "-vc14"
        builderified_name = 'blender-{}-{}-{}'.format(blender_full_version, git_hash, platform)
        # NOTE: Blender 2.7 is already respected by blender_full_version.
        if branch != '' and branch != 'blender2.7':
            builderified_name = branch + "-" + builderified_name

        os.rename(result_file, "{}.zip".format(builderified_name))
        # create zip file
        try:
            if os.path.exists(buildbot_upload_zip):
                os.remove(buildbot_upload_zip)
            z = zipfile.ZipFile(buildbot_upload_zip, "w", compression=zipfile.ZIP_STORED)
            z.write("{}.zip".format(builderified_name))
            z.close()
            sys.exit(retcode)
        except Exception as ex:
            sys.stderr.write('Create buildbot_upload.zip failed' + str(ex) + '\n')
            sys.exit(1)

    elif builder.startswith('linux_'):
        blender = os.path.join(install_dir, 'blender')

        buildinfo_h = os.path.join(build_dir, "source", "creator", "buildinfo.h")
        blender_h = os.path.join(blender_dir, "source", "blender", "blenkernel", "BKE_blender_version.h")

        # Get version information
        blender_version = int(parse_header_file(blender_h, 'BLENDER_VERSION'))
        blender_version = "%d.%d" % (blender_version // 100, blender_version % 100)
        blender_hash = parse_header_file(buildinfo_h, 'BUILD_HASH')[1:-1]
        blender_glibc = builder.split('_')[1]
        command_prefix = []

        if blender_glibc == 'glibc224':
            if builder.endswith('x86_64_cmake'):
                chroot_name = 'buildbot_stretch_x86_64'
                bits = 64
                blender_arch = 'x86_64'
            elif builder.endswith('i686_cmake'):
                chroot_name = 'buildbot_stretch_i686'
                bits = 32
                blender_arch = 'i686'
            command_prefix = ['schroot', '-c', chroot_name, '--']
        elif blender_glibc == 'glibc217':
            command_prefix = ['scl', 'enable', 'devtoolset-6', '--']

        # Strip all unused symbols from the binaries
        print("Stripping binaries...")
        subprocess.call(command_prefix + ['strip', '--strip-all', blender])

        print("Stripping python...")
        py_target = os.path.join(install_dir, blender_version)
        subprocess.call(command_prefix + ['find', py_target, '-iname', '*.so', '-exec', 'strip', '-s', '{}', ';'])

        # Copy all specific files which are too specific to be copied by
        # the CMake rules themselves
        print("Copying extra scripts and libs...")

        extra = '/' + os.path.join('home', 'sources', 'release-builder', 'extra')
        mesalibs = os.path.join(extra, 'mesalibs' + str(bits) + '.tar.bz2')
        software_gl = os.path.join(blender_dir, 'release', 'bin', 'blender-softwaregl')
        icons = os.path.join(blender_dir, 'release', 'freedesktop', 'icons')

        os.system('tar -xpf %s -C %s' % (mesalibs, install_dir))
        os.system('cp %s %s' % (software_gl, install_dir))
        os.system('cp -r %s %s' % (icons, install_dir))
        os.system('chmod 755 %s' % (os.path.join(install_dir, 'blender-softwaregl')))

        # Construct archive name
        package_name = 'blender-%s-%s-linux-%s-%s' % (blender_version,
                                                      blender_hash,
                                                      blender_glibc,
                                                      blender_arch)
        # NOTE: Blender 2.7 is already respected by blender_full_version.
        if branch != '' and branch != 'blender2.7':
            package_name = branch + "-" + package_name

        upload_filename = package_name + ".tar.bz2"

        print("Creating .tar.bz2 archive")
        upload_filepath = install_dir + '.tar.bz2'
        create_tar_bz2(install_dir, upload_filepath, package_name)
else:
    print("Unknown building system")
    sys.exit(1)


if upload_filepath is None:
    # clean release directory if it already exists
    release_dir = 'release'

    if os.path.exists(release_dir):
        for f in os.listdir(release_dir):
            if os.path.isfile(os.path.join(release_dir, f)):
                os.remove(os.path.join(release_dir, f))

    # create release package
    try:
        subprocess.call(['make', 'package_archive'])
    except Exception as ex:
        sys.stderr.write('Make package release failed' + str(ex) + '\n')
        sys.exit(1)

    # find release directory, must exist this time
    if not os.path.exists(release_dir):
        sys.stderr.write("Failed to find release directory %r.\n" % release_dir)
        sys.exit(1)

    # find release package
    file = None
    filepath = None

    for f in os.listdir(release_dir):
        rf = os.path.join(release_dir, f)
        if os.path.isfile(rf) and f.startswith('blender'):
            file = f
            filepath = rf

    if not file:
        sys.stderr.write("Failed to find release package.\n")
        sys.exit(1)

    upload_filename = file
    upload_filepath = filepath

# create zip file
try:
    upload_zip = os.path.join(buildbot_upload_zip)
    if os.path.exists(upload_zip):
        os.remove(upload_zip)
    z = zipfile.ZipFile(upload_zip, "w", compression=zipfile.ZIP_STORED)
    z.write(upload_filepath, arcname=upload_filename)
    z.close()
except Exception as ex:
    sys.stderr.write('Create buildbot_upload.zip failed' + str(ex) + '\n')
    sys.exit(1)
