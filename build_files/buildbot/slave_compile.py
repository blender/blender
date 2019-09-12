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

import buildbot_utils
import os
import shutil

def get_cmake_options(builder):
    config_file = "build_files/cmake/config/blender_release.cmake"
    options = ['-DCMAKE_BUILD_TYPE:STRING=Release', '-DWITH_GTESTS=ON']

    if builder.platform == 'mac':
        options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=x86_64')
        options.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=10.9')
    elif builder.platform == 'win':
        options.extend(['-G', 'Visual Studio 15 2017 Win64'])
    elif builder.platform == 'linux':
        config_file = "build_files/buildbot/config/blender_linux.cmake"

    optix_sdk_dir = os.path.join(builder.blender_dir, '..', '..', 'NVIDIA-Optix-SDK')
    options.append('-DOPTIX_ROOT_DIR:PATH=' + optix_sdk_dir)

    options.append("-C" + os.path.join(builder.blender_dir, config_file))
    options.append("-DCMAKE_INSTALL_PREFIX=%s" % (builder.install_dir))

    return options

def update_git(builder):
    # Do extra git fetch because not all platform/git/buildbot combinations
    # update the origin remote, causing buildinfo to detect local changes.
    os.chdir(builder.blender_dir)

    print("Fetching remotes")
    command = ['git', 'fetch', '--all']
    buildbot_utils.call(builder.command_prefix + command)

def clean_directories(builder):
    # Make sure no garbage remained from the previous run
    if os.path.isdir(builder.install_dir):
        shutil.rmtree(builder.install_dir)

    # Make sure build directory exists and enter it
    os.makedirs(builder.build_dir, exist_ok=True)

    # Remove buildinfo files to force buildbot to re-generate them.
    for buildinfo in ('buildinfo.h', 'buildinfo.h.txt', ):
        full_path = os.path.join(builder.build_dir, 'source', 'creator', buildinfo)
        if os.path.exists(full_path):
            print("Removing {}" . format(buildinfo))
            os.remove(full_path)

def cmake_configure(builder):
    # CMake configuration
    os.chdir(builder.build_dir)

    cmake_cache = os.path.join(builder.build_dir, 'CMakeCache.txt')
    if os.path.exists(cmake_cache):
        print("Removing CMake cache")
        os.remove(cmake_cache)

    print("CMake configure:")
    cmake_options = get_cmake_options(builder)
    command = ['cmake', builder.blender_dir] + cmake_options
    buildbot_utils.call(builder.command_prefix + command)

def cmake_build(builder):
    # CMake build
    os.chdir(builder.build_dir)

    if builder.platform == 'win':
        command = ['cmake', '--build', '.', '--target', 'install', '--config', 'Release']
    else:
        command = ['make', '-s', '-j2', 'install']

    print("CMake build:")
    buildbot_utils.call(builder.command_prefix + command)

if __name__ == "__main__":
    builder = buildbot_utils.create_builder_from_arguments()
    update_git(builder)
    clean_directories(builder)
    cmake_configure(builder)
    cmake_build(builder)
