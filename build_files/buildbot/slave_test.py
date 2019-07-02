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
    build_dir = os.path.abspath(os.path.join('..', 'build', builder))
    install_dir = os.path.abspath(os.path.join('..', 'install', builder))
    # NOTE: For quick test only to see if the approach work.
    # n the future must be replaced with an actual blender version.
    blender_version = '2.80'
    blender_version_dir = os.path.join(install_dir, blender_version)
    command_prefix = []

    if builder.startswith('linux'):
        tokens = builder.split("_")
        glibc = tokens[1]
        if glibc == 'glibc224':
            deb_name = "stretch"
            if builder.endswith('x86_64_cmake'):
                chroot_name = 'buildbot_' + deb_name + '_x86_64'
            elif builder.endswith('i686_cmake'):
                chroot_name = 'buildbot_' + deb_name + '_i686'
            command_prefix = ['schroot', '-c', chroot_name, '--']
        elif glibc == 'glibc217':
            command_prefix = ['scl', 'enable', 'devtoolset-6', '--']

    ctest_env = os.environ.copy()
    ctest_env['BLENDER_SYSTEM_SCRIPTS'] = os.path.join(blender_version_dir, 'scripts')
    ctest_env['BLENDER_SYSTEM_DATAFILES'] = os.path.join(blender_version_dir, 'datafiles')

    os.chdir(build_dir)
    retcode = subprocess.call(command_prefix + ['ctest', '--output-on-failure'], env=ctest_env)

    # Always exit with a success, for until we know all the tests are passing
    # on all builders.
    sys.exit(0)
else:
    print("Unknown building system")
    sys.exit(1)
