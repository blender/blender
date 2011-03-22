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

import os
import subprocess
import sys

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]

# we run from build/ directory
blender_dir = '../blender'

if builder.find('cmake') != -1:
    # cmake

    # set build options
    cmake_options = ['-DCMAKE_BUILD_TYPE:STRING=Release']

    if builder == 'mac_x86_64_cmake':
        cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=x86_64')
    elif builder == 'mac_i386_cmake':
        cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=i386')
    elif builder == 'mac_ppc_cmake':
        cmake_options.append('-DCMAKE_OSX_ARCHITECTURES:STRING=ppc')

    # configure and make
    retcode = subprocess.call(['cmake', blender_dir] + cmake_options)
    if retcode != 0:
        sys.exit(retcode)
    retcode = subprocess.call(['make', '-s', '-j4', 'install'])
    sys.exit(retcode)
else:
    # scons
    os.chdir(blender_dir)

    retcode = subprocess.call(['python', 'scons/scons.py'])
    sys.exit(retcode)

