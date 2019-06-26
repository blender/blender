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

import os
import subprocess
import sys
import shutil

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]

# we run from build/ directory
blender_dir = os.path.join('..', 'blender.git')


def parse_header_file(filename, define):
    import re
    regex = re.compile("^#\s*define\s+%s\s+(.*)" % define)
    with open(filename, "r") as file:
        for l in file:
            match = regex.match(l)
            if match:
                return match.group(1)
    return None

if 'cmake' in builder:
    # cmake

    # Some fine-tuning configuration
    blender_dir = os.path.abspath(blender_dir)
    build_dir = os.path.abspath(os.path.join('..', 'build', builder))
    install_dir = os.path.abspath(os.path.join('..', 'install', builder))
    targets = ['blender']

    chroot_name = None  # If not None command will be delegated to that chroot
    bits = 64

    # Config file to be used (relative to blender's sources root)
    cmake_config_file = "build_files/cmake/config/blender_release.cmake"

    # Set build options.
    cmake_options = []
    cmake_extra_options = ['-DCMAKE_BUILD_TYPE:STRING=Release']

    if builder.startswith('mac'):
        # Set up OSX architecture
        if builder.endswith('x86_64_10_9_cmake'):
            cmake_extra_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=x86_64')
        cmake_extra_options.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=10.9')

    elif builder.startswith('win'):
        if builder.startswith('win64'):
            cmake_options.extend(['-G', 'Visual Studio 15 2017 Win64'])
        elif builder.startswith('win32'):
            bits = 32
            cmake_options.extend(['-G', 'Visual Studio 15 2017'])

    elif builder.startswith('linux'):
        tokens = builder.split("_")
        glibc = tokens[1]
        if glibc == 'glibc224':
            deb_name = "stretch"
        elif glibc == 'glibc219':
            deb_name = "jessie"
        elif glibc == 'glibc211':
            deb_name = "squeeze"
        cmake_config_file = "build_files/buildbot/config/blender_linux.cmake"
        if builder.endswith('x86_64_cmake'):
            chroot_name = 'buildbot_' + deb_name + '_x86_64'
            targets = ['blender']
        elif builder.endswith('i686_cmake'):
            bits = 32
            chroot_name = 'buildbot_' + deb_name + '_i686'
            targets = ['blender']
        if deb_name != "stretch":
            cmake_extra_options.extend(["-DCMAKE_C_COMPILER=/usr/bin/gcc-7",
                                        "-DCMAKE_CXX_COMPILER=/usr/bin/g++-7"])

    cmake_options.append("-C" + os.path.join(blender_dir, cmake_config_file))

    # Prepare CMake options needed to configure cuda binaries compilation, 64bit only.
    if bits == 64:
        cmake_options.append("-DWITH_CYCLES_CUDA_BINARIES=ON")
        cmake_options.append("-DCUDA_64_BIT_DEVICE_CODE=ON")
    else:
        cmake_options.append("-DWITH_CYCLES_CUDA_BINARIES=OFF")

    cmake_options.append("-DCMAKE_INSTALL_PREFIX=%s" % (install_dir))

    cmake_options += cmake_extra_options

    # Prepare chroot command prefix if needed
    if chroot_name:
        chroot_prefix = ['schroot', '-c', chroot_name, '--']
    else:
        chroot_prefix = []

    # Make sure no garbage remained from the previous run
    if os.path.isdir(install_dir):
        shutil.rmtree(install_dir)

    for target in targets:
        print("Building target %s" % (target))
        # Construct build directory name based on the target
        target_build_dir = build_dir
        target_chroot_prefix = chroot_prefix[:]
        if target != 'blender':
            target_build_dir += '_' + target
        target_name = 'install'
        # Tweaking CMake options to respect the target
        target_cmake_options = cmake_options[:]
        # Do extra git fetch because not all platform/git/buildbot combinations
        # update the origin remote, causing buildinfo to detect local changes.
        os.chdir(blender_dir)
        print("Fetching remotes")
        command = ['git', 'fetch', '--all']
        print(command)
        retcode = subprocess.call(target_chroot_prefix + command)
        if retcode != 0:
            sys.exit(retcode)
        # Make sure build directory exists and enter it
        if not os.path.isdir(target_build_dir):
            os.mkdir(target_build_dir)
        os.chdir(target_build_dir)
        # Configure the build
        print("CMake options:")
        print(target_cmake_options)
        if os.path.exists('CMakeCache.txt'):
            print("Removing CMake cache")
            os.remove('CMakeCache.txt')
        # Remove buildinfo files to force buildbot to re-generate them.
        for buildinfo in ('buildinfo.h', 'buildinfo.h.txt', ):
            full_path = os.path.join('source', 'creator', buildinfo)
            if os.path.exists(full_path):
                print("Removing {}" . format(buildinfo))
                os.remove(full_path)
        retcode = subprocess.call(target_chroot_prefix + ['cmake', blender_dir] + target_cmake_options)
        if retcode != 0:
            print('Configuration FAILED!')
            sys.exit(retcode)

        if 'win32' in builder or 'win64' in builder:
            command = ['cmake', '--build', '.', '--target', target_name, '--config', 'Release']
        else:
            command = ['make', '-s', '-j2', target_name]

        print("Executing command:")
        print(command)
        retcode = subprocess.call(target_chroot_prefix + command)

        if retcode != 0:
            sys.exit(retcode)

else:
    print("Unknown building system")
    sys.exit(1)
