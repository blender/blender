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

def get_ctest_arguments(builder):
    args = ['--output-on-failure']
    if builder.platform == 'win':
        args += ['-C', 'Release']
    return args

def test(builder):
    os.chdir(builder.build_dir)

    command = builder.command_prefix  + ['ctest'] + get_ctest_arguments(builder)
    buildbot_utils.call(command)

if __name__ == "__main__":
    builder = buildbot_utils.create_builder_from_arguments()
    test(builder)
