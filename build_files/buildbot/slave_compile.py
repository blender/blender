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

if 'cmake' in builder:
    # cmake

    # Some fine-tuning configuration
    blender_dir = os.path.join('..', blender_dir)
    build_dir = os.path.abspath(os.path.join('..', 'build', builder))
    install_dir = os.path.abspath(os.path.join('..', 'install', builder))
    targets = ['blender']

    chroot_name = None  # If not None command will be delegated to that chroot
    build_cubins = True  # Whether to build Cycles CUDA kernels
    remove_cache = False  # Remove CMake cache to be sure config is totally up-to-date
    remove_install_dir = False  # Remove installation folder before building

    # Config file to be used (relative to blender's sources root)
    cmake_config_file = "build_files/cmake/config/blender_full.cmake"
    cmake_player_config_file = None
    cmake_cuda_config_file = None

    # Set build options.
    cmake_options = []
    cmake_extra_options = ['-DCMAKE_BUILD_TYPE:STRING=Release']

    if builder.startswith('mac'):
        remove_cache = True
        install_dir = None
        # Set up OSX architecture
        if builder.endswith('x86_64_10_6_cmake'):
            cmake_extra_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=x86_64')
        elif builder.endswith('i386_10_6_cmake'):
            build_cubins = False
            cmake_extra_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=i386')
            # Some special options to disable usupported features
            cmake_extra_options.append("-DWITH_CYCLES_OSL=OFF")
            cmake_extra_options.append("-DWITH_OPENCOLLADA=OFF")
        elif builder.endswith('ppc_10_6_cmake'):
            cmake_extra_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=ppc')

    elif builder.startswith('win'):
        install_dir = None
        if builder.startswith('win64'):
            cmake_options.append(['-G', '"Visual Studio 12 2013 Win64"'])
        elif builder.startswith('win32'):
            cmake_options.append(['-G', '"Visual Studio 12 2013"'])
            build_cubins = False

    elif builder.startswith('linux'):
        remove_cache = True
        remove_install_dir = True
        cmake_config_file = "build_files/buildbot/config/blender_linux.cmake"
        cmake_player_config_file = "build_files/buildbot/config/blender_linux_player.cmake"
        # Currently unused
        # cmake_cuda_config_file = "build_files/buildbot/config/blender_linux_cuda.cmake"
        if builder.endswith('x86_64_cmake'):
            chroot_name = 'buildbot_squeeze_x86_64'
            build_cubins = True
            targets = ['player', 'blender']
        elif builder.endswith('i386_cmake'):
            chroot_name = 'buildbot_squeeze_i686'
            build_cubins = False
            targets = ['player', 'blender']

    cmake_options.append("-C" + os.path.join(blender_dir, cmake_config_file))
    cmake_options.append("-DWITH_CYCLES_CUDA_BINARIES=%d" % (build_cubins))

    if install_dir:
        cmake_options.append("-DCMAKE_INSTALL_PREFIX=%s" % (install_dir))

    cmake_options += cmake_extra_options

    # Prepare chroot command prefix if needed

    if chroot_name:
        chroot_prefix = ['schroot', '-c', chroot_name, '--']
    else:
        chroot_prefix = []

    # Make sure no garbage remained from the previous run
    # (only do it if builder requested this)
    if remove_install_dir:
        if os.path.isdir(install_dir):
            shutil.rmtree(install_dir)

    for target in targets:
        print("Building target %s" % (target))
        # Construct build directory name based on the target
        target_build_dir = build_dir
        if target != 'blender':
            target_build_dir += '_' + target
        # Make sure build directory exists and enter it
        if not os.path.isdir(target_build_dir):
            os.mkdir(target_build_dir)
        os.chdir(target_build_dir)
        # Tweaking CMake options to respect the target
        target_cmake_options = cmake_options[:]
        if target == 'player':
            target_cmake_options.append("-C" + os.path.join(blender_dir, cmake_player_config_file))
        elif target == 'cuda':
            target_cmake_options.append("-C" + os.path.join(blender_dir, cmake_cuda_config_file))
        # Configure the build
        print("CMake options:")
        print(target_cmake_options)
        if remove_cache and os.path.exists('CMakeCache.txt'):
            print("Removing CMake cache")
            os.remove('CMakeCache.txt')
        retcode = subprocess.call(chroot_prefix + ['cmake', blender_dir] + target_cmake_options)
        if retcode != 0:
            print('Condifuration FAILED!')
            sys.exit(retcode)

        if 'win32' in builder:
            command = ['msbuild', 'INSTALL.vcxproj', '/Property:PlatformToolset=v120_xp', '/p:Configuration=Release']
        elif 'win64' in builder:
            command = ['msbuild', 'INSTALL.vcxproj', '/p:Configuration=Release']
        else:
            command = chroot_prefix + ['make', '-s', '-j2', 'install']

        print("Executing command:")
        print(command)
        retcode = subprocess.call(command)

        if retcode != 0:
            sys.exit(retcode)
else:
    print("Unknown building system")
    sys.exit(1)
