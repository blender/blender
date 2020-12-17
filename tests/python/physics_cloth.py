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

        MeshTest("ClothSimple", "testClothPlane", "expectedClothPlane",
                 [ModifierSpec('Cloth', 'CLOTH', {'settings': {'quality': 5}}, 15)], threshold=1e-3),

        # Not reproducible
        # MeshTest("ClothPressure", "testObjClothPressure", "expObjClothPressure",
        #           [ModifierSpec('Cloth2', 'CLOTH', {'settings': {'use_pressure': True,
        #           'uniform_pressure_force': 1}}, 16)]),

        # Not reproducible
        # MeshTest("ClothSelfCollision", "testClothCollision", "expClothCollision",
        #           [ModifierSpec('Cloth', 'CLOTH', {'collision_settings': {'use_self_collision': True}}, 67)]),

        MeshTest("ClothSpring", "testTorusClothSpring", "expTorusClothSpring",
                 [ModifierSpec('Cloth2', 'CLOTH', {'settings': {'use_internal_springs': True}}, 10)], threshold=1e-3),

    ]
    cloth_test = RunTest(test)

    command = list(sys.argv)
    for i, cmd in enumerate(command):
        if cmd == "--run-all-tests":
            cloth_test.apply_modifiers = True
            cloth_test.do_compare = True
            cloth_test.run_all_tests()
            break
        elif cmd == "--run-test":
            cloth_test.apply_modifiers = False
            cloth_test.do_compare = False
            name = command[i + 1]
            cloth_test.run_test(name)
            break


if __name__ == "__main__":
    main()
