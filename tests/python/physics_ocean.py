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
import sys

import bpy

sys.path.append(os.path.dirname(os.path.realpath(__file__)))
from modules.mesh_test import RunTest, ModifierSpec, MeshTest


def main():
    test = [
        # World coordinates of test and expected object should be same.
        MeshTest("PlaneOcean", "testObjPlaneOcean", "expObjPlaneOcean",
                 [ModifierSpec('Ocean', 'OCEAN', {})]),
    ]
    ocean_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            ocean_test.apply_modifiers = True
            ocean_test.do_compare = True
            ocean_test.run_all_tests()
            break
        elif cmd == "--run-test":
            ocean_test.apply_modifiers = False
            ocean_test.do_compare = False
            name = command[i + 1]
            ocean_test.run_test(name)
            break


if __name__ == "__main__":
    main()
