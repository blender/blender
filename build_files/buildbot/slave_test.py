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

import subprocess
import os
import sys

# get builder name
if len(sys.argv) < 2:
    sys.stderr.write("Not enough arguments, expecting builder name\n")
    sys.exit(1)

builder = sys.argv[1]

# we run from build/ directory
blender_dir = '../blender.git'

if "cmake" in builder:
    # cmake

    print("Automated tests are still DISABLED!")
    sys.exit(0)

    build_dir = os.path.abspath(os.path.join('..', 'build', builder))
    chroot_name = None
    chroot_prefix = []

    """
    if builder.endswith('x86_64_cmake'):
        chroot_name = 'buildbot_jessie_x86_64'
    elif builder.endswith('i686_cmake'):
        chroot_name = 'buildbot_jessie_i686'
    if chroot_name:
        chroot_prefix = ['schroot', '-c', chroot_name, '--']
    """

    os.chdir(build_dir)
    retcode = subprocess.call(chroot_prefix + ['ctest', '--output-on-failure'])
    sys.exit(retcode)
else:
    print("Unknown building system")
    sys.exit(1)
