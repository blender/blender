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
import sys

def get_ctest_environment(builder):
    info = buildbot_utils.VersionInfo(builder)
    blender_version_dir = os.path.join(builder.install_dir, info.version)

    env = os.environ.copy()
    env['BLENDER_SYSTEM_SCRIPTS'] = os.path.join(blender_version_dir, 'scripts')
    env['BLENDER_SYSTEM_DATAFILES'] = os.path.join(blender_version_dir, 'datafiles')
    return env

def get_ctest_arguments(builder):
    args = ['--output-on-failure']
    if builder.platform == 'win':
        args += ['-C', 'Release']
    return args

def test(builder):
    os.chdir(builder.build_dir)

    command = builder.command_prefix  + ['ctest'] + get_ctest_arguments(builder)
    ctest_env = get_ctest_environment(builder)
    buildbot_utils.call(command, env=ctest_env, exit_on_error=False)

if __name__ == "__main__":
    print("Automated tests are still DISABLED!")
    sys.exit(0)

    builder = buildbot_utils.create_builder_from_arguments()
    test(builder)

    # Always exit with a success, for until we know all the tests are passing
    # on all builders.
    sys.exit(0)
